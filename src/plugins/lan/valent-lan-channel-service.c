// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-channel-service"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <valent.h>

#include "valent-lan-channel.h"
#include "valent-lan-channel-service.h"
#include "valent-lan-utils.h"

#define IDENTITY_BUFFER_MAX  (8192)
#define IDENTITY_TIMEOUT_MAX (1000)


struct _ValentLanChannelService
{
  ValentChannelService  parent_instance;

  GTlsCertificate      *certificate;

  GNetworkMonitor      *monitor;
  gboolean              network_available;

  /* Service */
  uint16_t              port;
  uint16_t              tcp_port;
  char                 *broadcast_address;
  GSocketService       *listener;
  GMainLoop            *udp_context;
  GSocket              *udp_socket4;
  GSocket              *udp_socket6;
  GHashTable           *channels;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentLanChannelService, valent_lan_channel_service, VALENT_TYPE_CHANNEL_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))

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

static void
on_channel_destroyed (ValentLanChannelService *self,
                      ValentLanChannel        *channel)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  const char *device_id = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (VALENT_IS_LAN_CHANNEL (channel));

  certificate = valent_lan_channel_ref_peer_certificate (channel);
  device_id = valent_certificate_get_common_name (certificate);

  valent_object_lock (VALENT_OBJECT (self));
  if (g_hash_table_remove (self->channels, device_id))
    g_signal_handlers_disconnect_by_data (channel, self);
  valent_object_unlock (VALENT_OBJECT (self));
}

/**
 * valent_lan_channel_service_verify_channel:
 * @self: a #ValentLanChannelService
 * @identity: a KDE Connect identity packet
 * @connection: a #GTlsConnection
 *
 * Verify an encrypted TLS connection.
 *
 * @device_id should be the `deviceID` field from an identity packet. If it does
 * not match the common name for the peer certificate, %FALSE will be returned.
 *
 * @connection should be an encrypted TLS connection. If there is an existing
 * channel for @device_id with a different certificate, %FALSE will be returned.
 *
 * Returns: %TRUE if successful, or %FALSE on failure
 */
static gboolean
valent_lan_channel_service_verify_channel (ValentLanChannelService *self,
                                           JsonNode                *identity,
                                           GIOStream               *connection)
{
  ValentLanChannel *channel = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  const char *peer_certificate_cn = NULL;
  const char *device_id = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (self));
  g_assert (VALENT_IS_PACKET (identity));;
  g_assert (G_IS_TLS_CONNECTION (connection));

  if (!valent_packet_get_string (identity, "deviceId", &device_id))
    {
      g_debug ("%s(): expected \"deviceId\" field holding a string",
               G_STRFUNC);
      return FALSE;
    }

  g_object_get (connection, "peer-certificate", &peer_certificate, NULL);
  peer_certificate_cn = valent_certificate_get_common_name (peer_certificate);

  if (g_strcmp0 (device_id, peer_certificate_cn) != 0)
    {
      g_warning ("%s(): device ID does not match certificate common name",
                 G_STRFUNC);
      return FALSE;
    }

  valent_object_lock (VALENT_OBJECT (self));
  if ((channel = g_hash_table_lookup (self->channels, device_id)) != NULL)
    certificate = valent_lan_channel_ref_peer_certificate (channel);
  valent_object_unlock (VALENT_OBJECT (self));

  if (certificate && !g_tls_certificate_is_same (certificate, peer_certificate))
    {
      g_warning ("%s(): existing channel with different certificate",
                 G_STRFUNC);
      return FALSE;
    }

  return TRUE;
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
incoming_connection_timeout_cb (gpointer data)
{
  g_assert (G_IS_CANCELLABLE (data));

  g_cancellable_cancel ((GCancellable *)data);

  return G_SOURCE_REMOVE;
}

static gboolean
on_incoming_connection (ValentChannelService   *service,
                        GSocketConnection      *connection,
                        GCancellable           *cancellable,
                        GThreadedSocketService *listener)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autoptr (GCancellable) timeout = NULL;
  unsigned long cancellable_id = 0;
  g_autoptr (GSocketAddress) s_addr = NULL;
  GInetAddress *i_addr = NULL;
  g_autofree char *host = NULL;
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GError) warning = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  /* Timeout if the peer fails to authenticate in a timely fashion. */
  timeout = g_cancellable_new ();
  g_timeout_add_full (G_PRIORITY_DEFAULT,
                      IDENTITY_TIMEOUT_MAX,
                      incoming_connection_timeout_cb,
                      g_object_ref (timeout),
                      g_object_unref);

  if (cancellable != NULL)
    cancellable_id = g_cancellable_connect (cancellable,
                                            G_CALLBACK (g_cancellable_cancel),
                                            timeout,
                                            NULL);

  /* An incoming TCP connection is in response to an outgoing UDP packet, so the
   * the peer must now write its identity packet. */
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                             IDENTITY_BUFFER_MAX,
                                             timeout,
                                             &warning);

  if (peer_identity == NULL)
    {
      if (!g_error_matches (warning, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, warning->message);
      else if (!g_cancellable_is_cancelled (cancellable))
        g_warning ("%s(): timed out waiting for peer identity", G_STRFUNC);

      g_cancellable_disconnect (cancellable, cancellable_id);

      return TRUE;
    }

  /* Ignore identity packets without a deviceId */
  if (!valent_packet_get_string (peer_identity, "deviceId", &device_id))
    {
      g_debug ("%s(): expected \"deviceId\" field holding a string",
               G_STRFUNC);
      return TRUE;
    }

  VALENT_JSON (peer_identity, host);

  /* NOTE: We're the client when accepting incoming connections */
  valent_object_lock (VALENT_OBJECT (self));
  certificate = g_object_ref (self->certificate);
  valent_object_unlock (VALENT_OBJECT (self));

  tls_stream = valent_lan_encrypt_client_connection (connection,
                                                     certificate,
                                                     timeout,
                                                     &warning);

  if (tls_stream == NULL)
    {
      if (!g_error_matches (warning, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, warning->message);
      else if (!g_cancellable_is_cancelled (cancellable))
        g_warning ("%s(): timed out waiting for authentication", G_STRFUNC);

      g_cancellable_disconnect (cancellable, cancellable_id);

      return TRUE;
    }

  g_cancellable_disconnect (cancellable, cancellable_id);

  if (!valent_lan_channel_service_verify_channel (self, peer_identity, tls_stream))
    return TRUE;

  /* Get the host from the connection */
  s_addr = g_socket_connection_get_remote_address (connection, NULL);
  i_addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (s_addr));
  host = g_inet_address_to_string (i_addr);

  /* Create the new channel */
  identity = valent_channel_service_ref_identity (service);
  channel = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                          "base-stream",   tls_stream,
                          "host",          host,
                          "port",          self->port,
                          "identity",      identity,
                          "peer-identity", peer_identity,
                          NULL);

  valent_channel_service_channel (service, channel);

  return TRUE;
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
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GSocketService) listener = NULL;
  uint16_t tcp_port = VALENT_LAN_PROTOCOL_PORT;
  uint16_t tcp_port_max;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  /* The effective port range is actually relative to the construct-time
   *  `port`property, for running in test environments.
   */
  valent_object_lock (VALENT_OBJECT (self));
  tcp_port = self->port;
  tcp_port_max = tcp_port + (VALENT_LAN_PROTOCOL_PORT_MAX -
                             VALENT_LAN_PROTOCOL_PORT_MIN);
  valent_object_unlock (VALENT_OBJECT (self));

  /* Pass the service as the callback data for the "run" signal, while the
   * listener holds a reference to the object cancellable.
   */
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  listener = g_threaded_socket_service_new (g_get_num_processors ());
  g_signal_connect_object (listener,
                           "run",
                           G_CALLBACK (on_incoming_connection),
                           self,
                           G_CONNECT_SWAPPED);

  while (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (listener),
                                           tcp_port,
                                           G_OBJECT (destroy),
                                           error))
    {
      if (tcp_port >= tcp_port_max)
        {
          g_socket_service_stop (listener);
          g_socket_listener_close (G_SOCKET_LISTENER (listener));

          return FALSE;
        }

      g_clear_error (error);
      tcp_port++;
    }

  valent_object_lock (VALENT_OBJECT (self));
  self->tcp_port = tcp_port;
  self->listener = g_object_ref (listener);
  valent_channel_service_build_identity (VALENT_CHANNEL_SERVICE (self));
  valent_object_unlock (VALENT_OBJECT (self));

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
static void
incoming_broadcast_task (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (source_object);
  ValentChannelService *service = VALENT_CHANNEL_SERVICE (source_object);
  GSocketAddress *address = G_SOCKET_ADDRESS (task_data);
  JsonNode *peer_identity = NULL;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  GInetAddress *addr = NULL;
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (ValentChannel) channel = NULL;
  g_autofree char *host = NULL;
  int64_t port = VALENT_LAN_PROTOCOL_PORT;
  GOutputStream *output_stream;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (self));
  g_assert (G_IS_SOCKET_ADDRESS (address));

  addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));
  host = g_inet_address_to_string (addr);
  peer_identity = g_object_get_data (G_OBJECT (address), "valent-lan-broadcast");
  valent_packet_get_int (peer_identity, "tcpPort", &port);

  /* Open a TCP connection to the UDP sender and defined port.
   *
   * Disable the system proxy:
   *   - https://bugs.kde.org/show_bug.cgi?id=376187
   *   - https://github.com/andyholmes/gnome-shell-extension-gsconnect/issues/125
   */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  connection = g_socket_client_connect (client,
                                        G_SOCKET_CONNECTABLE (address),
                                        cancellable,
                                        &error);

  if (connection == NULL)
    {
      g_debug ("%s(): connecting to (%s:%"G_GINT64_FORMAT"): %s",
               G_STRFUNC, host, port, error->message);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  /* Write the local identity. Once we do this, both peers will have the ability
   * to authenticate or reject TLS certificates.
   */
  identity = valent_channel_service_ref_identity (service);
  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  if (!valent_packet_to_stream (output_stream, identity, cancellable, &error))
    {
      g_debug ("%s(): sending identity to (%s:%"G_GINT64_FORMAT"): %s",
               G_STRFUNC, host, port, error->message);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  /* NOTE: We're the server when opening outgoing connections */
  valent_object_lock (VALENT_OBJECT (self));
  certificate = g_object_ref (self->certificate);
  valent_object_unlock (VALENT_OBJECT (self));

  tls_stream = valent_lan_encrypt_server_connection (connection,
                                                     certificate,
                                                     cancellable,
                                                     &error);

  if (tls_stream == NULL)
    {
      g_debug ("%s(): authenticating (%s:%"G_GINT64_FORMAT"): %s",
               G_STRFUNC, host, port, error->message);
      return g_task_return_error (task, g_steal_pointer (&error));
    }

  if (!valent_lan_channel_service_verify_channel (self, peer_identity, tls_stream))
    return g_task_return_boolean (task, TRUE);

  channel = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                          "base-stream",   tls_stream,
                          "host",          host,
                          "port",          port,
                          "identity",      identity,
                          "peer-identity", peer_identity,
                          NULL);

  valent_channel_service_channel (service, channel);
  g_task_return_boolean (task, TRUE);
}

static gboolean
valent_lan_channel_service_socket_recv (GSocket      *socket,
                                        GIOCondition  condition,
                                        gpointer      user_data)
{
  ValentChannelService *service = VALENT_CHANNEL_SERVICE (user_data);
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GError) error = NULL;
  gssize read = 0;
  char buffer[IDENTITY_BUFFER_MAX + 1] = { 0, };
  g_autoptr (GSocketAddress) incoming = NULL;
  g_autoptr (GSocketAddress) outgoing = NULL;
  GInetAddress *addr = NULL;
  int64_t port = VALENT_LAN_PROTOCOL_PORT;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id;
  g_autofree char *local_id = NULL;
  g_autoptr (GTask) task = NULL;
  g_autoptr (GError) warning = NULL;

  g_assert (G_IS_SOCKET (socket));
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (user_data));

  if (condition != G_IO_IN)
    return G_SOURCE_REMOVE;

  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (service));

  /* Read the message data and extract the remote address */
  read = g_socket_receive_from (socket,
                                &incoming,
                                buffer,
                                IDENTITY_BUFFER_MAX,
                                cancellable,
                                &error);

  if (read == -1)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return G_SOURCE_REMOVE;
    }

  if (read == 0)
    {
      g_warning ("%s(): Socket is closed", G_STRFUNC);
      return G_SOURCE_REMOVE;
    }

  /* Validate the message as a KDE Connect packet */
  if ((peer_identity = valent_packet_deserialize (buffer, &warning)) == NULL)
    {
      g_warning ("%s(): failed to parse peer-identity: %s",
                 G_STRFUNC,
                 warning->message);
      return G_SOURCE_CONTINUE;
    }

  /* Ignore broadcasts without a deviceId or from ourselves */
  if (!valent_packet_get_string (peer_identity, "deviceId", &device_id))
    {
      g_debug ("%s(): expected \"deviceId\" field holding a string",
               G_STRFUNC);
      return G_SOURCE_CONTINUE;
    }

  local_id = valent_channel_service_dup_id (service);

  if (g_strcmp0 (device_id, local_id) == 0)
    return G_SOURCE_CONTINUE;

  VALENT_JSON (peer_identity, device_id);

  /* Get the remote address and port */
  addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (incoming));

  if (!valent_packet_get_int (peer_identity, "tcpPort", &port) ||
      (port < VALENT_LAN_PROTOCOL_PORT_MIN || port > VALENT_LAN_PROTOCOL_PORT_MAX))
    {
      g_debug ("%s(): expected \"tcpPort\" field holding a uint16 between %u-%u",
               G_STRFUNC,
               VALENT_LAN_PROTOCOL_PORT_MIN,
               VALENT_LAN_PROTOCOL_PORT_MAX);
      return G_SOURCE_CONTINUE;
    }

  /* Defer the remaining work to another thread */
  outgoing = g_inet_socket_address_new (addr, port);
  g_object_set_data_full (G_OBJECT (outgoing),
                          "valent-lan-broadcast",
                          json_node_ref (peer_identity),
                          (GDestroyNotify)json_node_unref);

  task = g_task_new (service, cancellable, NULL, NULL);
  g_task_set_source_tag (task, valent_lan_channel_service_socket_recv);
  g_task_set_task_data (task, g_steal_pointer (&outgoing), g_object_unref);
  g_task_run_in_thread (task, incoming_broadcast_task);

  return G_SOURCE_CONTINUE;
}

static gboolean
valent_lan_channel_service_socket_send (GSocket      *socket,
                                        GIOCondition  condition,
                                        gpointer      user_data)
{
  GSocketAddress *address = G_SOCKET_ADDRESS (user_data);
  GBytes *bytes = NULL;
  gssize written;
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_SOCKET (socket));
  g_assert (G_IS_SOCKET_ADDRESS (address));

  if (condition != G_IO_OUT)
    return G_SOURCE_REMOVE;

  bytes = g_object_get_data (G_OBJECT (address), "valent-lan-broadcast");
  written = g_socket_send_to (socket,
                              address,
                              g_bytes_get_data (bytes, NULL),
                              g_bytes_get_size (bytes),
                              NULL,
                              &error);

  /* We only check for real errors, not partial writes */
  if (written == -1)
    g_warning ("%s(): failed to identify: %s", G_STRFUNC, error->message);

  return G_SOURCE_REMOVE;
}

static void
valent_lan_channel_service_socket_queue (ValentLanChannelService *self,
                                         GSocketAddress          *address)
{
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (GBytes) identity_bytes = NULL;
  char *identity_json = NULL;
  GSocketFamily family = G_SOCKET_FAMILY_INVALID;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (G_IS_SOCKET_ADDRESS (address));

  /* Ignore errant socket addresses */
  family = g_socket_address_get_family (address);

  if (family != G_SOCKET_FAMILY_IPV4 && family != G_SOCKET_FAMILY_IPV6)
    g_return_if_reached ();

  /* Serialize the identity */
  identity = valent_channel_service_ref_identity (VALENT_CHANNEL_SERVICE (self));
  identity_json = valent_packet_serialize (identity);
  identity_bytes = g_bytes_new_take (identity_json, strlen (identity_json));
  g_object_set_data_full (G_OBJECT (address),
                          "valent-lan-broadcast",
                          g_bytes_ref (identity_bytes),
                          (GDestroyNotify)g_bytes_unref);

  valent_object_lock (VALENT_OBJECT (self));
  if ((self->udp_socket6 != NULL && family == G_SOCKET_FAMILY_IPV6) ||
      (self->udp_socket6 != NULL && g_socket_speaks_ipv4 (self->udp_socket6)))
    {
      g_autoptr (GSource) source = NULL;

      source = g_socket_create_source (self->udp_socket6, G_IO_OUT, NULL);
      g_source_set_callback (source,
                             G_SOURCE_FUNC (valent_lan_channel_service_socket_send),
                             g_object_ref (address),
                             g_object_unref);
      g_source_attach (source, g_main_loop_get_context (self->udp_context));
    }

  if (self->udp_socket4 != NULL && family == G_SOCKET_FAMILY_IPV4)
    {
      g_autoptr (GSource) source = NULL;

      source = g_socket_create_source (self->udp_socket4, G_IO_OUT, NULL);
      g_source_set_callback (source,
                             G_SOURCE_FUNC (valent_lan_channel_service_socket_send),
                             g_object_ref (address),
                             g_object_unref);
      g_source_attach (source, g_main_loop_get_context (self->udp_context));
    }
  valent_object_unlock (VALENT_OBJECT (self));
}

static gpointer
valent_lan_channel_service_socket_worker (gpointer data)
{
  g_autoptr (GMainLoop) loop = (GMainLoop *)data;
  GMainContext *context = g_main_loop_get_context (loop);

  /* The loop quits when the channel is closed, then the context is drained to
   * ensure all tasks return. */
  g_main_context_push_thread_default (context);

  g_main_loop_run (loop);

  while (g_main_context_pending (context))
    g_main_context_iteration (NULL, FALSE);

  g_main_context_pop_thread_default (context);

  return NULL;
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
  g_autoptr (GMainContext) context = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (GThread) thread = NULL;
  g_autoptr (GSocket) socket4 = NULL;
  g_autoptr (GSocket) socket6 = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  uint16_t port = VALENT_LAN_PROTOCOL_PORT;

  VALENT_ENTRY;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (cancellable == NULL || G_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Create a thread with a GMainContext to manage traffic */
  context = g_main_context_new ();
  loop = g_main_loop_new (context, FALSE);
  thread = g_thread_try_new ("valent-lan-channel-service",
                             valent_lan_channel_service_socket_worker,
                             g_main_loop_ref (loop),
                             error);

  if (thread == NULL)
    {
      g_main_loop_unref (loop);
      VALENT_RETURN (FALSE);
    }

  /* Prepare socket(s) for UDP-based discovery */
  valent_object_lock (VALENT_OBJECT (self));
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  port = self->port;
  valent_object_unlock (VALENT_OBJECT (self));

  /* Prefer dual IPv4/IPv6 socket */
  socket6 = g_socket_new (G_SOCKET_FAMILY_IPV6,
                          G_SOCKET_TYPE_DATAGRAM,
                          G_SOCKET_PROTOCOL_UDP,
                          NULL);

  if (socket6 != NULL)
    {
      g_autoptr (GInetAddress) inet_address = NULL;
      g_autoptr (GSocketAddress) address = NULL;
      g_autoptr (GSource) source = NULL;

      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV6);
      address = g_inet_socket_address_new (inet_address, port);

      if (!g_socket_bind (socket6, address, TRUE, NULL))
        {
          g_clear_object (&socket6);
          VALENT_GOTO (ipv4);
        }

      g_socket_set_broadcast (socket6, TRUE);

      /* Watch for incoming broadcasts */
      source = g_socket_create_source (socket6, G_IO_IN, destroy);
      g_source_set_callback (source,
                             G_SOURCE_FUNC (valent_lan_channel_service_socket_recv),
                             g_object_ref (self),
                             g_object_unref);
      g_source_attach (source, context);

      /* If this socket also speaks IPv4 then we are done. */
      if (g_socket_speaks_ipv4 (socket6))
        VALENT_GOTO (check);
    }

ipv4:
  socket4 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                          G_SOCKET_TYPE_DATAGRAM,
                          G_SOCKET_PROTOCOL_UDP,
                          error);

  if (socket4 != NULL)
    {
      g_autoptr (GInetAddress) inet_address = NULL;
      g_autoptr (GSocketAddress) address = NULL;
      g_autoptr (GSource) source = NULL;

      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
      address = g_inet_socket_address_new (inet_address, port);

      if (!g_socket_bind (socket4, address, TRUE, error))
        {
          g_clear_object (&socket4);
          VALENT_GOTO (check);
        }

      g_socket_set_broadcast (socket4, TRUE);

      /* Watch for incoming broadcasts */
      source = g_socket_create_source (socket4, G_IO_IN, destroy);
      g_source_set_callback (source,
                             G_SOURCE_FUNC (valent_lan_channel_service_socket_recv),
                             g_object_ref (self),
                             g_object_unref);
      g_source_attach (source, context);
    }

check:
  if (socket4 == NULL && socket6 == NULL)
    VALENT_RETURN (FALSE);
  else
    g_clear_error (error);

  valent_object_lock (VALENT_OBJECT (self));
  self->udp_context = g_main_loop_ref (loop);
  self->udp_socket4 = g_steal_pointer (&socket4);
  self->udp_socket6 = g_steal_pointer (&socket6);
  valent_object_unlock (VALENT_OBJECT (self));

  VALENT_RETURN (TRUE);
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
      json_object_set_int_member (body, "tcpPort", self->tcp_port);
    }
}

static void
valent_lan_channel_service_channel (ValentChannelService *service,
                                    ValentChannel        *channel)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  ValentLanChannel *lan_channel = VALENT_LAN_CHANNEL (channel);
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  const char *device_id = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (VALENT_IS_LAN_CHANNEL (lan_channel));

  peer_certificate = valent_lan_channel_ref_peer_certificate (lan_channel);
  device_id = valent_certificate_get_common_name (peer_certificate);

  valent_object_lock (VALENT_OBJECT (service));
  g_hash_table_replace (self->channels, g_strdup (device_id), channel);
  g_signal_connect_object (channel,
                           "destroy",
                           G_CALLBACK (on_channel_destroyed),
                           self,
                           G_CONNECT_SWAPPED);
  valent_object_unlock (VALENT_OBJECT (service));
}

static void
valent_lan_channel_service_identify (ValentChannelService *service,
                                     const char           *target)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);
  g_autoptr (GSocketAddress) address = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  if (!self->network_available)
    return;

  if (target != NULL)
    {
      g_autoptr (GSocketConnectable) naddr = NULL;
      const char *hostname = NULL;
      uint16_t port = 0;
      g_autoptr (GError) error = NULL;

      naddr = g_network_address_parse (target,
                                       VALENT_LAN_PROTOCOL_PORT,
                                       &error);

      if (naddr == NULL)
        {
          g_warning ("%s(): failed to parse \"%s\": %s",
                     G_STRFUNC,
                     target,
                     error->message);
          return;
        }

      hostname = g_network_address_get_hostname (G_NETWORK_ADDRESS (naddr));
      port = g_network_address_get_port (G_NETWORK_ADDRESS (naddr));
      address = g_inet_socket_address_new_from_string (hostname, port);
    }

  if (address == NULL)
    address = g_inet_socket_address_new_from_string (self->broadcast_address,
                                                     self->port);

  valent_lan_channel_service_socket_queue (self, address);
}

static void
valent_lan_channel_service_init_task (GTask        *task,
                                      gpointer      source_object,
                                      gpointer      task_data,
                                      GCancellable *cancellable)
{
  ValentLanChannelService *self = source_object;
  g_autoptr (GError) error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (!valent_lan_channel_service_tcp_setup (self, cancellable, &error) ||
      !valent_lan_channel_service_udp_setup (self, cancellable, &error))
    return g_task_return_error (task, g_steal_pointer (&error));


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

  g_task_run_in_thread (task, valent_lan_channel_service_init_task);
}

/*
 * GAsyncInitable
 */
static void
valent_lan_channel_service_init_async (GAsyncInitable      *initable,
                                       int                  io_priority,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (initable);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GFile) config_dir = NULL;
  g_autoptr (ValentContext) context = NULL;
  ValentContext *root_context = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (initable));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* Cancel initialization if the object is destroyed */
  destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
                                              cancellable);

  self->network_available = g_network_monitor_get_network_available (self->monitor);
  g_signal_connect_object (self->monitor,
                           "network-changed",
                           G_CALLBACK (on_network_changed),
                           self, 0);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_lan_channel_service_init_async);

  // TODO: the certificate is in the root context
  g_object_get (initable, "context", &context, NULL);
  root_context = valent_context_get_root (context);
  config_dir = valent_context_get_config_file (root_context, ".");

  valent_certificate_new (g_file_peek_path (config_dir),
                          destroy,
                          valent_certificate_new_cb,
                          g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_lan_channel_service_init_async;
}


/*
 * GObject
 */
static void
valent_lan_channel_service_dispose (GObject *object)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

  g_signal_handlers_disconnect_by_data (self->monitor, self);

  valent_object_lock (VALENT_OBJECT (self));
  if (self->udp_context != NULL)
    {
      g_clear_object (&self->udp_socket4);
      g_clear_object (&self->udp_socket6);
      g_main_loop_quit (self->udp_context);
      g_clear_pointer (&self->udp_context, g_main_loop_unref);
    }

  if (self->listener != NULL)
    {
      g_socket_service_stop (G_SOCKET_SERVICE (self->listener));
      g_socket_listener_close (G_SOCKET_LISTENER (self->listener));
      g_clear_object (&self->listener);
    }
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_lan_channel_service_parent_class)->dispose (object);
}

static void
valent_lan_channel_service_finalize (GObject *object)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

  g_clear_object (&self->certificate);
  g_clear_pointer (&self->broadcast_address, g_free);
  g_clear_pointer (&self->channels, g_hash_table_unref);

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

  object_class->dispose = valent_lan_channel_service_dispose;
  object_class->finalize = valent_lan_channel_service_finalize;
  object_class->get_property = valent_lan_channel_service_get_property;
  object_class->set_property = valent_lan_channel_service_set_property;

  service_class->build_identity = valent_lan_channel_service_build_identity;
  service_class->channel = valent_lan_channel_service_channel;
  service_class->identify = valent_lan_channel_service_identify;

  /**
   * ValentLanChannelService:broadcast-address:
   *
   * The UDP broadcast address for the service.
   *
   * This available as a construct property primarily for use in unit tests.
   */
  properties [PROP_BROADCAST_ADDRESS] =
    g_param_spec_string ("broadcast-address", NULL, NULL,
                         VALENT_LAN_PROTOCOL_ADDR,
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
   * The TCP/IP port for the service.
   *
   * The current KDE Connect protocol (v7) defines port 1716 as the default.
   *
   * This available as a construct property primarily for use in unit tests.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port", NULL, NULL,
                       VALENT_LAN_PROTOCOL_PORT_MIN, VALENT_LAN_PROTOCOL_PORT_MAX,
                       VALENT_LAN_PROTOCOL_PORT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_lan_channel_service_init (ValentLanChannelService *self)
{
  self->channels = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          NULL);
  self->monitor = g_network_monitor_get_default ();
}

