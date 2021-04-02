// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-channel-service"

#include "config.h"

#include <gio/gunixinputstream.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-lan-channel.h"
#include "valent-lan-channel-service.h"
#include "valent-lan-utils.h"

#define DEFAULT_PORT      1716
#define TRANSFER_PORT_MIN 1739
#define TRANSFER_PORT_MAX 1764


struct _ValentLanChannelService
{
  ValentChannelService  parent_instance;

  GCancellable         *cancellable;
  GSettings            *settings;

  GNetworkMonitor      *monitor;
  gboolean              network_available;

  char                 *broadcast_address;
  GTlsCertificate      *certificate;
  guint                 port;
  GSocketService       *listener;

  /* UDP */
  GSocket              *udp_socket4;
  GSocket              *udp_socket6;
};

G_DEFINE_TYPE (ValentLanChannelService, valent_lan_channel_service, VALENT_TYPE_CHANNEL_SERVICE)

enum {
  PROP_0,
  PROP_BROADCAST_ADDRESS,
  PROP_CERTIFICATE,
  PROP_PORT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
on_network_changed (GNetworkMonitor         *monitor,
                    gboolean                 network_available,
                    ValentLanChannelService *self)
{
  if (self->network_available == network_available)
    return;

  if ((self->network_available = network_available))
    valent_channel_service_identify (VALENT_CHANNEL_SERVICE (self), NULL);
}

/*
 * Incoming Connections
 *
 * When an incoming connection is opened to the TCP listener, we are operating
 * as the client. The server expects us to:
 *
 * 1) Accept the TCP connection
 * 2) Read the peer identity packet
 * 3) Negotiate TLS encryption (as the TLS Client)
 */
static gboolean
on_connection (GThreadedSocketService *listener,
               GSocketConnection      *connection,
               GObject                *source_object)
{
  ValentChannelService *service = VALENT_CHANNEL_SERVICE (source_object);
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autofree char *host = NULL;
  g_autoptr (GSocketAddress) saddr = NULL;
  GInetAddress *iaddr;
  JsonNode *identity;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id;
  g_autoptr (GIOStream) tls_stream = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  g_autofree char *uri = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  /* The incoming TCP connection is in response to an outgoing UDP packet, so the peer must now
   * write its identity packet. */
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                             self->cancellable,
                                             NULL);

  if (peer_identity == NULL)
    return TRUE;

  /* Now that we have the device ID we can authorize or reject certificates.
   * NOTE: We're the client when accepting incoming connections */
  device_id = valent_identity_get_device_id (peer_identity);
  tls_stream = valent_lan_encrypt_new_client (connection,
                                              self->certificate,
                                              device_id,
                                              self->cancellable,
                                              NULL);

  if (tls_stream == NULL)
    return TRUE;

  /* Get the host from the connection */
  saddr = g_socket_connection_get_remote_address (connection, NULL);
  iaddr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (saddr));
  host = g_inet_address_to_string (iaddr);

  /* Create the new channel */
  identity = valent_channel_service_get_identity (service);
  uri = g_strdup_printf ("lan://%s:%u", host, self->port);
  channel = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                          "base-stream",   tls_stream,
                          "certificate",   self->certificate,
                          "host",          host,
                          "identity",      identity,
                          "peer-identity", peer_identity,
                          "port",          self->port,
                          "uri",           uri,
                          NULL);

  valent_channel_service_emit_channel (service, channel);

  return TRUE;
}

/**
 * peek_host:
 * @socket: a #GSocket
 * @error: (nullable): a #GError
 *
 * Peek the host from the next UDP message on a socket. Returns %NULL on error
 * and sets @error.
 *
 * Returns: (transfer full): the remote host as a string
 */
static char *
peek_host (GSocket       *socket,
           GCancellable  *cancellable,
           GError       **error)
{
  g_autoptr (GSocketAddress) address;
  GInetAddress *iaddr;
  g_autofree GInputVector iv;
  iv.buffer = NULL;
  iv.size = 0;
  gint flags = G_SOCKET_MSG_PEEK;
  gssize len;

  g_assert (G_IS_SOCKET (socket));
  g_assert (error == NULL || *error == NULL);

  len = g_socket_receive_message (socket,
                                  &address,
                                  &iv, 0,
                                  NULL, NULL,
                                  &flags,
                                  cancellable,
                                  error);

  if (len == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           "UDP socket closed");
      return NULL;
    }
  else if (len == -1)
    {
      return NULL;
    }

  iaddr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));

  return g_inet_address_to_string (iaddr);
}

/*
 * Incoming UDP Broadcasts
 *
 * When an identity packet is received over a UDP port (usually a broadcast), we
 * are operating as the server. The client expects us to:
 *
 * 1) Open a TCP connection
 * 2) Write our identity packet
 * 3) Negotiate TLS encryption (as the TLS Server)
 */
static gboolean
on_packet (ValentLanChannelService  *self,
           GSocket                  *socket,
           GDataInputStream         *input_stream,
           GCancellable             *cancellable,
           GError                  **error)
{
  ValentChannelService *service = VALENT_CHANNEL_SERVICE (self);
  g_autoptr (GError) warn = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  guint16 port;
  g_autofree char *host = NULL;
  g_autofree char *uri = NULL;
  g_autofree char *line = NULL;
  JsonNode *identity;
  g_autoptr (JsonNode) peer_identity = NULL;
  JsonObject *body;
  const char *device_id;
  const char *local_id;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  GOutputStream *output_stream;
  GIOStream *tls_stream;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (G_IS_SOCKET (socket));

  /* Try to peek the incoming address before we read the packet, and on failure
   * assume it's a broken pipe.
   */
  host = peek_host (socket, cancellable, error);

  if G_UNLIKELY (host == NULL)
    return FALSE;

  /* We assume there's an identity packet and now we'll confirm it. Read from
   * the UDP socket until the next line-feed character, parse it as JSON, and
   * get the port while validating the packet.
   */
  line = g_data_input_stream_read_line (input_stream, NULL, cancellable, error);

  if G_UNLIKELY (line == NULL)
    {
      if (error != NULL && *error == NULL)
        g_set_error_literal (error,
                             G_IO_ERROR,
                             G_IO_ERROR_CONNECTION_CLOSED,
                             "UDP socket closed");
      return FALSE;
    }

  peer_identity = valent_packet_deserialize (line, &warn);

  if G_UNLIKELY (peer_identity == NULL)
    {
      g_warning ("[%s] Parsing peer-identity: %s", G_STRFUNC, warn->message);
      return TRUE;
    }

  /* Ignore broadcasts without a deviceId or from ourselves */
  body = valent_packet_get_body (peer_identity);
  device_id = json_object_get_string_member_with_default (body, "deviceId", NULL);

  if G_UNLIKELY (device_id == NULL)
    {
      g_warning ("[%s] Missing `deviceId` field", G_STRFUNC);
      return TRUE;
    }

  local_id = valent_channel_service_get_id (service);

  if G_UNLIKELY (g_strcmp0 (device_id, local_id) == 0)
    return TRUE;

  /* Get the remote port */
  port = (guint16)json_object_get_int_member_with_default (body, "tcpPort", 0);

  if G_UNLIKELY (port == 0)
    {
      g_debug ("Missing `tcpPort` field");
      return TRUE;
    }

  VALENT_DEBUG_PACKET (peer_identity, "Received Identity");


  /* Open a TCP connection to the UDP sender and defined port. Disable any use
   * of the system proxy.
   *
   * https://bugs.kde.org/show_bug.cgi?id=376187
   * https://github.com/andyholmes/gnome-shell-extension-gsconnect/issues/125
   */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  connection = g_socket_client_connect_to_host (client,
                                                host,
                                                port,
                                                cancellable,
                                                &warn);

  if (connection == NULL)
    {
      g_debug ("Error connecting (%s:%u): %s", host, port, warn->message);
      return TRUE;
    }

  /* Write the local identity. Once we do this, both peers will have the ability
   * to authenticate or reject TLS certificates.
   */
  identity = valent_channel_service_get_identity (service);
  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  if (!valent_packet_to_stream (output_stream, identity, cancellable, error))
    {
      g_debug ("Error writing identity (%s:%u): %s", host, port, warn->message);
      return TRUE;
    }

  /* We're the TLS Server when responding to identity broadcasts */
  tls_stream = valent_lan_encrypt_new_server (connection,
                                              self->certificate,
                                              device_id,
                                              cancellable,
                                              &warn);

  if (tls_stream == NULL)
    {
      g_debug ("Error authenticating (%s:%u): %s", host, port, warn->message);
      return TRUE;
    }

  /* Create new channel */
  uri = g_strdup_printf ("lan://%s:%u", host, port);
  channel = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                          "base-stream",   tls_stream,
                          "certificate",   self->certificate,
                          "host",          host,
                          "port",          port,
                          "uri",           uri,
                          "identity",      identity,
                          "peer-identity", peer_identity,
                          NULL);

  valent_channel_service_emit_channel (service, channel);

  return TRUE;
}

typedef struct
{
  ValentLanChannelService *service;
  GSocket                 *socket;
  GCancellable            *cancellable;
} SocketThreadData;

static gpointer
socket_read_loop (gpointer data)
{
  SocketThreadData *info = data;
  ValentLanChannelService *self = info->service;
  GSocket *socket = info->socket;
  GCancellable *cancellable = info->cancellable;
  g_autoptr (GInputStream) base_stream = NULL;
  g_autoptr (GDataInputStream) input_stream = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (info->service));
  g_assert (G_IS_SOCKET (info->socket));

  /* Prepare a stream for reading identity packets */
  base_stream = g_unix_input_stream_new (g_socket_get_fd (socket), TRUE);
  input_stream = g_data_input_stream_new (base_stream);

  while (g_socket_condition_wait (socket, G_IO_IN, cancellable, &error))
    {
      if (!on_packet (self, socket, input_stream, cancellable, &error))
        break;
    }

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("[%s] %s", G_STRFUNC, error->message);

  g_clear_object (&info->service);
  g_clear_object (&info->socket);
  g_clear_object (&info->cancellable);
  g_clear_pointer (&info, g_free);

  return NULL;
}

/**
 * valent_lan_channel_service_ensure_certificate:
 * @self: a #ValentLanChannelService
 * @error: (nullable): a #GError
 *
 * Check if a TLS certificate has exists for the backend and attempt to generate
 * one if not. Returns %FALSE on error and sets @error.
 *
 * Returns: boolean indicating success
 */
static gboolean
valent_lan_channel_service_ensure_certificate (ValentLanChannelService  *self,
                                               GError                  **error)
{
  ValentChannelService *service = VALENT_CHANNEL_SERVICE (self);
  g_autoptr (ValentData) data = NULL;
  g_autoptr (GTlsCertificate) certificate= NULL;
  g_autoptr (GFile) cert_file = NULL;
  g_autoptr (GFile) key_file = NULL;
  const char *cert_path;
  const char *key_path;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  if (G_IS_TLS_CERTIFICATE (self->certificate))
    return TRUE;

  /* Check if the certificate has already been generated */
  g_object_get (self, "data", &data, NULL);
  cert_file = valent_data_new_config_file (data, "certificate.pem");
  key_file = valent_data_new_config_file (data, "private.pem");

  cert_path = g_file_peek_path (cert_file);
  key_path = g_file_peek_path (key_file);

  /* Generate new certificate with gnutls */
  if (!g_file_query_exists (cert_file, NULL) ||
      !g_file_query_exists (key_file, NULL))
    {
      const char *local_id;

      local_id = valent_channel_service_get_id (service);

      if (!valent_certificate_generate (key_path, cert_path, local_id, error))
        return FALSE;
    }

  certificate = g_tls_certificate_new_from_files (cert_path, key_path, error);

  if (certificate != NULL)
    g_set_object (&self->certificate, certificate);

  return G_IS_TLS_CERTIFICATE (self->certificate);
}

/**
 * valent_lan_channel_service_tcp_setup:
 * @self: a #ValentLanChannelService
 * @error: (nullable): a #GError
 *
 * A wrapper around g_socket_listener_add_inet_port() that can be called
 * multiple times.
 *
 * Returns: %TRUE or %FALSE with @error set
 */
static gboolean
valent_lan_channel_service_tcp_setup (ValentLanChannelService  *self,
                                      GCancellable             *cancellable,
                                      GError                  **error)
{
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (self->port != 0);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  /* Create a listener and override the ::run vfunc */
  self->listener = g_threaded_socket_service_new (10);
  G_THREADED_SOCKET_SERVICE_GET_CLASS (self->listener)->run = on_connection;

  /* Start listening for connections */
  if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (self->listener),
                                        self->port,
                                        G_OBJECT (self),
                                        error))
    {
      g_socket_service_stop (self->listener);
      g_socket_listener_close (G_SOCKET_LISTENER (self->listener));
      g_clear_object (&self->listener);
    }

  return G_IS_SOCKET_SERVICE (self->listener);
}

/**
 * valent_lan_channel_service_udp_setup:
 * @self: a #ValentLanChannelService
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * An analog to valent_lan_channel_service_tcp_setup() that prepares UDP sockets for IPv4
 * and IPv6, including streams for reading.
 *
 * Returns: %TRUE or %FALSE with @error set
 */
static gboolean
valent_lan_channel_service_udp_setup (ValentLanChannelService  *self,
                                      GCancellable             *cancellable,
                                      GError                  **error)
{
  g_autoptr (GSocket) socket4 = NULL;
  g_autoptr (GSocket) socket6 = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (self->port != 0);

  if (self->udp_socket6 || self->udp_socket4)
    return TRUE;

  /* first try to create an IPv6 socket */
  socket6 = g_socket_new (G_SOCKET_FAMILY_IPV6,
                          G_SOCKET_TYPE_DATAGRAM,
                          G_SOCKET_PROTOCOL_UDP,
                          NULL);

  if (socket6 != NULL)
    {
      g_autoptr (GInetAddress) inet_address = NULL;
      g_autoptr (GSocketAddress) address = NULL;
      g_autoptr (GThread) thread = NULL;
      SocketThreadData *data;

      /* Bind the port */
      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV6);
      address = g_inet_socket_address_new (inet_address, self->port);

      if (g_socket_bind (socket6, address, TRUE, error))
        g_socket_set_broadcast (socket6, TRUE);
      else
        return FALSE;

      /* Watch the socket for incoming identity packets */
      data = g_new0 (SocketThreadData, 1);
      data->service = g_object_ref (self);
      data->socket = g_object_ref (socket6);
      data->cancellable = g_object_ref (cancellable);
      thread = g_thread_new (NULL, socket_read_loop, data);

      self->udp_socket6 = g_steal_pointer (&socket6);

      /* If this socket also speaks IPv4 then we are done. */
      if (g_socket_speaks_ipv4 (self->udp_socket6))
        return TRUE;
    }

  /* We need an IPv4 socket, either instead or in addition to our IPv6 */
  socket4 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                          G_SOCKET_TYPE_DATAGRAM,
                          G_SOCKET_PROTOCOL_UDP,
                          error);

  if (socket4 != NULL)
    {
      g_autoptr (GInetAddress) inet_address = NULL;
      g_autoptr (GSocketAddress) address = NULL;
      g_autoptr (GThread) thread = NULL;
      SocketThreadData *data;

      /* Bind the port */
      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
      address = g_inet_socket_address_new (inet_address, self->port);

      if (g_socket_bind (socket4, address, TRUE, error))
        g_socket_set_broadcast (socket4, TRUE);
      else
        return FALSE;

      /* Watch the socket for incoming identity packets */
      data = g_new0 (SocketThreadData, 1);
      data->service = g_object_ref (self);
      data->socket = g_object_ref (socket4);
      data->cancellable = g_object_ref (cancellable);
      thread = g_thread_new (NULL, socket_read_loop, data);

      self->udp_socket4 = g_steal_pointer (&socket4);
    }
  else
    {
      if (self->udp_socket6 == NULL)
        return FALSE;

      g_clear_error (error);
    }

  return TRUE;
}


/*
 * ValentChannelService
 */
static void
valent_lan_channel_service_build_identity (ValentChannelService *service)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  ValentChannelServiceClass *klass;
  JsonNode *identity;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (service));

  /* Chain-up */
  klass = VALENT_CHANNEL_SERVICE_CLASS (valent_lan_channel_service_parent_class);
  klass->build_identity (service);

  /* Set the tcpPort on the packet */
  identity = valent_channel_service_get_identity (service);

  if (identity != NULL)
    {
      JsonObject *root;
      JsonObject *body;

      root = json_node_get_object (identity);
      g_assert (json_object_has_member (root, "body"));

      body = json_object_get_object_member (root, "body");
      json_object_set_int_member (body, "tcpPort", self->port);
    }
}

static void
valent_lan_channel_service_identify (ValentChannelService *service,
                                     const char           *target)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autoptr (GError) error = NULL;
  g_autoptr (GNetworkAddress) naddr = NULL;
  g_autoptr (GSocketAddress) address = NULL;
  JsonNode *identity;
  g_autofree char *identity_json = NULL;
  glong identity_len;
  const char *hostname;
  guint16 port;
  gssize written;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  if (!self->network_available)
    return;

  if (target != NULL)
    {
      naddr = G_NETWORK_ADDRESS (g_network_address_parse (target,
                                                          DEFAULT_PORT,
                                                          &error));

      if (naddr == NULL)
        {
          g_warning ("Failed to parse address %s: %s", target, error->message);
          return;
        }

      hostname = g_network_address_get_hostname (naddr);
      port = g_network_address_get_port (naddr);
    }
  else
    {
      hostname = self->broadcast_address;
      port = self->port;
    }

  address = g_inet_socket_address_new_from_string (hostname, port);

  /* Serialize the identity */
  identity = valent_channel_service_get_identity (service);
  identity_json = valent_packet_serialize (identity);
  identity_len = strlen (identity_json);

  /* IPv6 */
  if (self->udp_socket6 != NULL)
    {
      g_autoptr (GError) error = NULL;

      written = g_socket_send_to (self->udp_socket6,
                                  address,
                                  identity_json,
                                  identity_len,
                                  NULL,
                                  &error);

      /* We only check for real errors, not partial writes */
      if (written == -1)
        g_debug ("Failed to identify to %s: %s", target, error->message);
    }

  /* IPv4 */
  if (self->udp_socket4 != NULL)
    {
      g_autoptr (GError) error = NULL;

      written = g_socket_send_to (self->udp_socket4,
                                  address,
                                  identity_json,
                                  identity_len,
                                  NULL,
                                  &error);

      /* We only check for real errors, not partial writes */
      if (written == -1)
        g_debug ("Failed to identify to %s: %s", target, error->message);
    }
}

static void
start_task (GTask        *task,
            gpointer      source_object,
            gpointer      task_data,
            GCancellable *cancellable)
{
  ValentLanChannelService *self = source_object;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  /* Ensure certificate */
  if (!valent_lan_channel_service_ensure_certificate (self, &error))
    return g_task_return_error (task, error);

  /* TCP Listener */
  if (!valent_lan_channel_service_tcp_setup (self, cancellable, &error))
    return g_task_return_error (task, error);

  /* UDP Socket(s) */
  if (!valent_lan_channel_service_udp_setup (self, cancellable, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_lan_channel_service_start (ValentChannelService *service,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (service));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             self->cancellable,
                             G_CONNECT_SWAPPED);

  self->network_available = g_network_monitor_get_network_available (self->monitor);
  g_signal_connect (self->monitor,
                    "network-changed",
                    G_CALLBACK (on_network_changed),
                    self);

  task = g_task_new (service, self->cancellable, callback, user_data);
  g_task_run_in_thread (task, start_task);
}

static void
valent_lan_channel_service_stop (ValentChannelService *service)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  /* Network Monitor */
  g_signal_handlers_disconnect_by_data (self->monitor, self);

  /* TCP Listener */
  if (self->listener != NULL)
    {
      g_socket_service_stop (G_SOCKET_SERVICE (self->listener));
      g_socket_listener_close (G_SOCKET_LISTENER (self->listener));
      g_clear_object (&self->listener);
    }

  /* UDP Sockets */
  g_clear_object (&self->udp_socket4);
  g_clear_object (&self->udp_socket6);
}


/*
 * GObject
 */
static void
valent_lan_channel_service_constructed (GObject *object)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

  if (self->broadcast_address == NULL)
    self->broadcast_address = g_strdup ("255.255.255.255");

  G_OBJECT_CLASS (valent_lan_channel_service_parent_class)->constructed (object);
}

static void
valent_lan_channel_service_finalize (GObject *object)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

  g_clear_object (&self->certificate);
  g_clear_pointer (&self->broadcast_address, g_free);
  g_clear_object (&self->listener);
  g_clear_object (&self->udp_socket4);
  g_clear_object (&self->udp_socket6);

  G_OBJECT_CLASS (valent_lan_channel_service_parent_class)->finalize (object);
}

static void
valent_lan_channel_service_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

  switch (prop_id)
    {
    case PROP_BROADCAST_ADDRESS:
      g_value_set_string (value, self->broadcast_address);
      break;

    case PROP_CERTIFICATE:
      g_value_set_object (value, valent_lan_channel_service_get_certificate (self));
      break;

    case PROP_PORT:
      g_value_set_uint (value, self->port);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_channel_service_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

  switch (prop_id)
    {
    case PROP_BROADCAST_ADDRESS:
      self->broadcast_address = g_value_dup_string (value);
      break;

    case PROP_PORT:
      self->port = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_channel_service_class_init (ValentLanChannelServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  object_class->constructed = valent_lan_channel_service_constructed;
  object_class->finalize = valent_lan_channel_service_finalize;
  object_class->get_property = valent_lan_channel_service_get_property;
  object_class->set_property = valent_lan_channel_service_set_property;

  service_class->build_identity = valent_lan_channel_service_build_identity;
  service_class->identify = valent_lan_channel_service_identify;
  service_class->start = valent_lan_channel_service_start;
  service_class->stop = valent_lan_channel_service_stop;

  /**
   * ValentLanChannelService:broadcast-address:
   *
   * The UDP broadcast address for the backend.
   *
   * This available as a construct property primarily for use in unit tests.
   */
  properties [PROP_BROADCAST_ADDRESS] =
    g_param_spec_string ("broadcast-address",
                         "Broadcast Address",
                         "The UDP broadcast address for outgoing identity packets",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannelService:certificate:
   *
   * The TLS certificate the backend uses to authenticate with other devices.
   */
  properties [PROP_CERTIFICATE] =
    g_param_spec_object ("certificate",
                         "Certificate",
                         "TLS Certificate",
                         G_TYPE_TLS_CERTIFICATE,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannelService:port:
   *
   * The TCP/IP port for the backend. The current KDE Connect protocol (v7)
   * defines port 1716 as the default.
   *
   * This available as a construct property primarily for use in unit tests.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port",
                       "Port",
                       "TCP/IP port",
                       0, G_MAXUINT16,
                       DEFAULT_PORT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_lan_channel_service_init (ValentLanChannelService *self)
{
  self->cancellable = NULL;
  self->certificate = NULL;
  self->monitor = g_network_monitor_get_default ();
  self->broadcast_address = NULL;
  self->port = DEFAULT_PORT;
}

/**
 * valent_lan_channel_service_get_certificate:
 *
 * Ensure a TLS certificate and private key exists for the service and return it
 * as a #GTlsCertificate. If either the certificate or private key are missing
 * a new certificate-key pair will be generated.
 *
 * Any failure to generate or retrieve the certificate-key pair is a fatal error
 * for Valent.
 *
 * Returns: (transfer none): the service #GTlsCertificate
 */
GTlsCertificate *
valent_lan_channel_service_get_certificate (ValentLanChannelService *self)
{
  g_return_val_if_fail (VALENT_IS_LAN_CHANNEL_SERVICE (self), NULL);

  if G_UNLIKELY (self->certificate == NULL)
    {
      g_autoptr (GError) error = NULL;

      if (!valent_lan_channel_service_ensure_certificate (self, &error))
        g_critical ("Failed to generate certificate: %s", error->message);
    }

  return self->certificate;
}

