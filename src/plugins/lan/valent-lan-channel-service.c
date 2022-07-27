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

#define PROTOCOL_ADDR        "255.255.255.255"
#define PROTOCOL_PORT        (1716)
#define TRANSFER_PORT_MIN    (1739)
#define TRANSFER_PORT_MAX    (1764)

#define IDENTITY_BUFFER_MAX  (8192)


struct _ValentLanChannelService
{
  ValentChannelService  parent_instance;

  GCancellable         *cancellable;
  GTlsCertificate      *certificate;

  GNetworkMonitor      *monitor;
  gboolean              network_available;

  /* Service */
  guint16               port;
  char                 *broadcast_address;
  GSocketService       *listener;
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
 * Incoming TCP Connections
 *
 * When an incoming connection is opened to the TCP listener, we are operating
 * as the client. The server expects us to:
 *
 * 1) Accept the TCP connection
 * 2) Read the peer identity packet
 * 3) Negotiate TLS encryption (as the TLS Client)
 */
static gboolean
on_incoming_connection (ValentChannelService   *service,
                        GSocketConnection      *connection,
                        GCancellable           *cancellable,
                        GThreadedSocketService *listener)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autofree char *host = NULL;
  g_autoptr (GSocketAddress) saddr = NULL;
  GInetAddress *iaddr;
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id;
  g_autoptr (GIOStream) tls_stream = NULL;
  g_autoptr (ValentChannel) channel = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  /* An incoming TCP connection is in response to an outgoing UDP packet, so the
   * the peer must now write its identity packet. */
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                             -1,
                                             cancellable,
                                             NULL);

  if (peer_identity == NULL)
    return TRUE;

  /* Ignore identity packets without a deviceId */
  if (!valent_packet_get_string (peer_identity, "deviceId", &device_id))
    {
      g_warning ("%s(): expected \"deviceId\" field holding a string",
                 G_STRFUNC);
      return TRUE;
    }

  /* NOTE: We're the client when accepting incoming connections */
  valent_object_lock (VALENT_OBJECT (self));
  tls_stream = valent_lan_encrypt_new_client (connection,
                                              self->certificate,
                                              cancellable,
                                              NULL);
  valent_object_unlock (VALENT_OBJECT (self));

  if (tls_stream == NULL)
    return TRUE;

  /* Get the host from the connection */
  saddr = g_socket_connection_get_remote_address (connection, NULL);
  iaddr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (saddr));
  host = g_inet_address_to_string (iaddr);

  /* Create the new channel */
  identity = valent_channel_service_ref_identity (service);
  channel = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                          "base-stream",   tls_stream,
                          "certificate",   self->certificate,
                          "host",          host,
                          "identity",      identity,
                          "peer-identity", peer_identity,
                          "port",          self->port,
                          NULL);

  valent_channel_service_emit_channel (service, channel);

  return TRUE;
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
on_incoming_broadcast (ValentLanChannelService  *self,
                       GSocket                  *socket,
                       GCancellable             *cancellable,
                       GError                  **error)
{
  ValentChannelService *service = VALENT_CHANNEL_SERVICE (self);
  g_autoptr (GError) warn = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GSocketAddress) address;
  char buffer[IDENTITY_BUFFER_MAX + 1] = { 0, };
  gssize len;
  GInetAddress *iaddr;
  g_autofree char *host = NULL;
  gint64 port;
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id;
  g_autofree char *local_id = NULL;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  GOutputStream *output_stream;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (G_IS_SOCKET (socket));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Read the message data and extract the remote address */
  len = g_socket_receive_from (socket,
                               &address,
                               buffer,
                               IDENTITY_BUFFER_MAX,
                               cancellable,
                               error);

  if (len == 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           "UDP socket closed");
      return FALSE;
    }
  else if (len == -1)
    {
      return FALSE;
    }

  /* Validate the message as a KDE Connect packet */
  if ((peer_identity = valent_packet_deserialize (buffer, &warn)) == NULL)
    {
      g_warning ("%s(): failed to parse peer-identity: %s",
                 G_STRFUNC,
                 warn->message);
      return TRUE;
    }

  /* Ignore broadcasts without a deviceId or from ourselves */
  if (!valent_packet_get_string (peer_identity, "deviceId", &device_id))
    {
      g_warning ("%s(): expected \"deviceId\" field holding a string",
                 G_STRFUNC);
      return TRUE;
    }

  local_id = valent_channel_service_dup_id (service);

  if (g_strcmp0 (device_id, local_id) == 0)
    return TRUE;

  /* Get the remote host and port */
  iaddr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));
  host = g_inet_address_to_string (iaddr);

  if (valent_packet_get_int (peer_identity, "tcpPort", &port) &&
      (port < 0 || port > G_MAXUINT16))
    {
      g_warning ("%s(): expected \"tcpPort\" field holding a uint16",
                 G_STRFUNC);
      return TRUE;
    }

  VALENT_JSON (peer_identity, "Peer Identity");

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
      g_debug ("%s(): connecting to (%s:%"G_GINT64_FORMAT"): %s",
               G_STRFUNC, host, port, warn->message);
      return TRUE;
    }

  /* Write the local identity. Once we do this, both peers will have the ability
   * to authenticate or reject TLS certificates.
   */
  identity = valent_channel_service_ref_identity (service);
  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  if (!valent_packet_to_stream (output_stream, identity, cancellable, error))
    {
      g_debug ("%s(): sending identity to (%s:%"G_GINT64_FORMAT"): %s",
               G_STRFUNC, host, port, warn->message);
      return TRUE;
    }

  /* We're the TLS Server when responding to identity broadcasts */
  valent_object_lock (VALENT_OBJECT (self));
  tls_stream = valent_lan_encrypt_new_server (connection,
                                              self->certificate,
                                              cancellable,
                                              &warn);
  valent_object_unlock (VALENT_OBJECT (self));

  if (tls_stream == NULL)
    {
      g_debug ("%s(): authenticating (%s:%"G_GINT64_FORMAT"): %s",
               G_STRFUNC, host, port, warn->message);
      return TRUE;
    }

  /* Create new channel */
  channel = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                          "base-stream",   tls_stream,
                          "certificate",   self->certificate,
                          "host",          host,
                          "port",          port,
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
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (info->service));
  g_assert (G_IS_SOCKET (info->socket));

  /* Prepare a stream for reading identity packets */
  while (g_socket_condition_wait (socket, G_IO_IN, cancellable, &error))
    {
      if (!on_incoming_broadcast (self, socket, cancellable, &error))
        break;
    }

  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s(): %s", G_STRFUNC, error->message);

  g_clear_object (&info->service);
  g_clear_object (&info->socket);
  g_clear_object (&info->cancellable);
  g_clear_pointer (&info, g_free);

  return NULL;
}

/**
 * valent_lan_channel_service_tcp_setup:
 * @self: a #ValentLanChannelService
 * @error: (nullable): a #GError
 *
 * A wrapper around g_socket_listener_add_inet_port() that can be called
 * multiple times.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
static gboolean
valent_lan_channel_service_tcp_setup (ValentLanChannelService  *self,
                                      GCancellable             *cancellable,
                                      GError                  **error)
{
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  valent_object_lock (VALENT_OBJECT (self));

  /* Pass the service as the callback data for the "run" signal, while the
   * listener holds a reference to the cancellable for this "start" sequence.
   */
  self->listener = g_threaded_socket_service_new (10);
  g_signal_connect_swapped (self->listener,
                            "run",
                            G_CALLBACK (on_incoming_connection),
                            self);

  if (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (self->listener),
                                        self->port,
                                        G_OBJECT (cancellable),
                                        error))
    {
      g_socket_service_stop (self->listener);
      g_socket_listener_close (G_SOCKET_LISTENER (self->listener));
      g_clear_object (&self->listener);
    }

  valent_object_unlock (VALENT_OBJECT (self));

  return G_IS_SOCKET_SERVICE (self->listener);
}

/**
 * valent_lan_channel_service_udp_setup:
 * @self: a #ValentLanChannelService
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * An analog to valent_lan_channel_service_tcp_setup() that prepares UDP sockets
 * for IPv4 and IPv6, including streams for reading.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
static gboolean
valent_lan_channel_service_udp_setup (ValentLanChannelService  *self,
                                      GCancellable             *cancellable,
                                      GError                  **error)
{
  g_autoptr (GSocket) socket4 = NULL;
  g_autoptr (GSocket) socket6 = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  valent_object_lock (VALENT_OBJECT (self));

  if (self->udp_socket6 || self->udp_socket4)
    {
      valent_object_unlock (VALENT_OBJECT (self));
      return TRUE;
    }

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
        {
          g_socket_set_broadcast (socket6, TRUE);
        }
      else
        {
          valent_object_unlock (VALENT_OBJECT (self));
          return FALSE;
        }

      /* Watch the socket for incoming identity packets */
      data = g_new0 (SocketThreadData, 1);
      data->service = g_object_ref (self);
      data->socket = g_object_ref (socket6);
      data->cancellable = g_object_ref (cancellable);
      thread = g_thread_new (NULL, socket_read_loop, data);

      self->udp_socket6 = g_steal_pointer (&socket6);

      /* If this socket also speaks IPv4 then we are done. */
      if (g_socket_speaks_ipv4 (self->udp_socket6))
        {
          valent_object_unlock (VALENT_OBJECT (self));
          return TRUE;
        }
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
        {
          g_socket_set_broadcast (socket4, TRUE);
        }
      else
        {
          valent_object_unlock (VALENT_OBJECT (self));
          return FALSE;
        }

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
        {
          valent_object_unlock (VALENT_OBJECT (self));
          return FALSE;
        }

      g_clear_error (error);
    }

  valent_object_unlock (VALENT_OBJECT (self));
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
  g_autoptr (JsonNode) identity = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (service));

  /* Chain-up */
  klass = VALENT_CHANNEL_SERVICE_CLASS (valent_lan_channel_service_parent_class);
  klass->build_identity (service);

  /* Set the tcpPort on the packet */
  identity = valent_channel_service_ref_identity (service);

  if (identity != NULL)
    {
      JsonObject *body;

      body = valent_packet_get_body (identity);
      json_object_set_int_member (body, "tcpPort", self->port);
    }
}

static void
valent_lan_channel_service_identify (ValentChannelService *service,
                                     const char           *target)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autoptr (GNetworkAddress) naddr = NULL;
  g_autoptr (GSocketAddress) address = NULL;
  g_autoptr (JsonNode) identity = NULL;
  g_autofree char *identity_json = NULL;
  glong identity_len;
  const char *hostname;
  guint16 port;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  if (!self->network_available)
    return;

  if (target != NULL)
    {
      g_autoptr (GError) error = NULL;

      naddr = G_NETWORK_ADDRESS (g_network_address_parse (target,
                                                          PROTOCOL_PORT,
                                                          &error));

      if (naddr == NULL)
        {
          g_warning ("%s(): failed to parse \"%s\": %s",
                     G_STRFUNC,
                     target,
                     error->message);
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
  identity = valent_channel_service_ref_identity (service);
  identity_json = valent_packet_serialize (identity);
  identity_len = strlen (identity_json);

  /* IPv6 */
  if (self->udp_socket6 != NULL)
    {
      gssize written;
      g_autoptr (GError) error = NULL;

      written = g_socket_send_to (self->udp_socket6,
                                  address,
                                  identity_json,
                                  identity_len,
                                  NULL,
                                  &error);

      /* We only check for real errors, not partial writes */
      if (written == -1)
        g_warning ("%s(): failed to identify to \"%s\": %s",
                   G_STRFUNC,
                   target,
                   error->message);
    }

  /* IPv4 */
  if (self->udp_socket4 != NULL)
    {
      gssize written;
      g_autoptr (GError) error = NULL;

      written = g_socket_send_to (self->udp_socket4,
                                  address,
                                  identity_json,
                                  identity_len,
                                  NULL,
                                  &error);

      /* We only check for real errors, not partial writes */
      if (written == -1)
        g_warning ("%s(): failed to identify to \"%s\": %s",
                   G_STRFUNC,
                   target,
                   error->message);
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

  /* TCP Listener */
  if (!valent_lan_channel_service_tcp_setup (self, cancellable, &error))
    return g_task_return_error (task, error);

  /* UDP Socket(s) */
  if (!valent_lan_channel_service_udp_setup (self, cancellable, &error))
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
valent_certificate_new_cb (GObject      *object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentLanChannelService *self = g_task_get_source_object (task);
  GError *error = NULL;

  valent_object_lock (VALENT_OBJECT (self));
  self->certificate = valent_certificate_new_finish (result, &error);
  valent_object_unlock (VALENT_OBJECT (self));
  g_task_run_in_thread (task, start_task);
}

static void
valent_lan_channel_service_start (ValentChannelService *service,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autoptr (ValentData) data = NULL;
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
  g_task_set_source_tag (task, valent_lan_channel_service_start);

  g_object_get (service, "data", &data, NULL);
  valent_certificate_new (valent_data_get_config_path (data),
                          self->cancellable,
                          valent_certificate_new_cb,
                          g_steal_pointer (&task));
}

static void
valent_lan_channel_service_stop (ValentChannelService *service)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  if (self->cancellable == NULL)
    return;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  /* Network Monitor */
  g_signal_handlers_disconnect_by_data (self->monitor, self);

  valent_object_lock (VALENT_OBJECT (self));

  if (self->listener != NULL)
    {
      g_socket_service_stop (G_SOCKET_SERVICE (self->listener));
      g_socket_listener_close (G_SOCKET_LISTENER (self->listener));
      g_clear_object (&self->listener);
    }

  g_clear_object (&self->udp_socket4);
  g_clear_object (&self->udp_socket6);

  valent_object_unlock (VALENT_OBJECT (self));
}


/*
 * GObject
 */
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
      g_value_set_object (value, self->certificate);
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
   * The UDP broadcast address for the service.
   *
   * This available as a construct property primarily for use in unit tests.
   */
  properties [PROP_BROADCAST_ADDRESS] =
    g_param_spec_string ("broadcast-address", NULL, NULL,
                         PROTOCOL_ADDR,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannelService:certificate:
   *
   * The TLS certificate the service uses to authenticate with other devices.
   */
  properties [PROP_CERTIFICATE] =
    g_param_spec_object ("certificate", NULL, NULL,
                         G_TYPE_TLS_CERTIFICATE,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannelService:port:
   *
   * The TCP/IP port for the service. The current KDE Connect protocol (v7)
   * defines port 1716 as the default.
   *
   * This available as a construct property primarily for use in unit tests.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port", NULL, NULL,
                       1024, G_MAXUINT16,
                       PROTOCOL_PORT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_lan_channel_service_init (ValentLanChannelService *self)
{
  self->monitor = g_network_monitor_get_default ();
}

