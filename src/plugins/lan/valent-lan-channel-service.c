// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-channel-service"

#include "config.h"

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libdex.h>
#include <valent.h>

#include "valent-lan-channel.h"
#include "valent-lan-channel-service.h"
#include "valent-lan-dnssd.h"
#include "valent-lan-utils.h"

#define HANDSHAKE_TIMEOUT_MS (1000)
#define IDENTITY_BUFFER_MAX  (8192)

#if (VALENT_SANITIZE_ADDRESS || VALENT_SANITIZE_THREAD)
#undef HANDSHAKE_TIMEOUT_MS
#define HANDSHAKE_TIMEOUT_MS (10000)
#endif

struct _ValentLanChannelService
{
  ValentChannelService  parent_instance;

  GNetworkMonitor      *monitor;
  gboolean              network_available;

  /* Service */
  uint16_t              port;
  uint16_t              tcp_port;
  char                 *broadcast_address;
  GListModel           *dnssd;
  GSocketService       *listener;
  GSocket              *udp_socket4;
  GSocket              *udp_socket6;
  GHashTable           *channels;
};

static void   g_initable_iface_init (GInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentLanChannelService, valent_lan_channel_service, VALENT_TYPE_CHANNEL_SERVICE,
                               G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, g_initable_iface_init))

typedef enum {
  PROP_BROADCAST_ADDRESS = 1,
  PROP_PORT,
} ValentLanChannelServiceProperty;

static GParamSpec *properties[PROP_PORT + 1] = { NULL, };

static void
on_network_changed (GNetworkMonitor         *monitor,
                    gboolean                 network_available,
                    ValentLanChannelService *self)
{
  if (self->network_available == network_available)
    return;

  self->network_available = network_available;
  if (self->network_available)
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_ACTIVE,
                                             NULL);
    }
  else
    {
      valent_extension_plugin_state_changed (VALENT_EXTENSION (self),
                                             VALENT_PLUGIN_STATE_INACTIVE,
                                             NULL);
    }
}

static void
on_channel_destroyed (ValentLanChannelService *self,
                      ValentLanChannel        *channel)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  const char *device_id = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (VALENT_IS_LAN_CHANNEL (channel));

  certificate = valent_channel_ref_certificate (VALENT_CHANNEL (channel));
  device_id = valent_certificate_get_common_name (certificate);
  g_hash_table_remove (self->channels, device_id);
}

/*
 * Connection Handshake
 *
 * The handshake begins after the discovered device receives an identity packet.
 * In protocol v7 this simply means negotiating a TLS connection, while
 * protocol v8 requires re-exchanging identity packets over the encrypted
 * connection.
 */
typedef struct
{
  ValentChannelService *service;
  GIOStream            *connection;
  JsonNode             *peer_identity;
  char                 *host;
  uint16_t              port;
  GCancellable         *cancellable;
  unsigned long         cancellable_id;
  GCancellable         *task_cancellable;
  GSource              *timeout;
} HandshakeData;

static void
handshake_cancelled_cb (GCancellable *cancellable,
                        gpointer      data)
{
  g_assert (G_IS_CANCELLABLE (data));

  g_cancellable_cancel ((GCancellable *)data);
}

static gboolean
handshake_timeout_cb (gpointer data)
{
  g_assert (G_IS_CANCELLABLE (data));

  g_cancellable_cancel ((GCancellable *)data);

  return G_SOURCE_REMOVE;
}

static HandshakeData *
handshake_data_new (ValentChannelService *service)
{
  HandshakeData *ret = NULL;
  g_autoptr (GMainContext) context = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));

  context = g_main_context_ref_thread_default ();

  ret = g_new0 (HandshakeData, 1);
  ret->service = g_object_ref (service);

  ret->task_cancellable = g_cancellable_new ();
  ret->cancellable = valent_object_ref_cancellable (VALENT_OBJECT (service));
  ret->cancellable_id = g_cancellable_connect (ret->cancellable,
                                               G_CALLBACK (handshake_cancelled_cb),
                                               g_object_ref (ret->task_cancellable),
                                               g_object_unref);

  ret->timeout = g_timeout_source_new (HANDSHAKE_TIMEOUT_MS);
  g_source_set_priority (ret->timeout, G_PRIORITY_HIGH);
  g_source_set_static_name (ret->timeout, "[valent-lan-plugin] handshake timeout");
  g_source_set_callback (ret->timeout,
                         handshake_timeout_cb,
                         g_object_ref (ret->task_cancellable),
                         g_object_unref);
  g_source_attach (ret->timeout, context);

  return ret;
}

static void
handshake_data_free (gpointer user_data)
{
  HandshakeData *data = (HandshakeData *)user_data;

  if (data->cancellable != NULL)
    {
      g_cancellable_disconnect (data->cancellable, data->cancellable_id);
      g_clear_object (&data->cancellable);
    }

  if (data->timeout != NULL)
    {
      g_source_destroy (data->timeout);
      g_clear_pointer (&data->timeout, g_source_unref);
    }

  g_clear_object (&data->service);
  g_clear_object (&data->connection);
  g_clear_pointer (&data->peer_identity, json_node_unref);
  g_clear_pointer (&data->host, g_free);
  g_clear_object (&data->task_cancellable);
  g_free (data);
}

static void
g_socket_client_connect_to_host_cb (GSocketClient *client,
                                    GAsyncResult  *result,
                                    gpointer       user_data)
{
  g_autoptr (DexPromise) promise = DEX_PROMISE (g_steal_pointer (&user_data));
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GError) error = NULL;

  connection = g_socket_client_connect_to_host_finish (client, result, &error);
  if (connection == NULL)
    dex_promise_reject (promise, g_steal_pointer (&error));
  else
    dex_promise_resolve_object (promise, g_steal_pointer (&connection));
}

static DexFuture *
_dex_socket_client_connect_to_host (const char   *host,
                                    guint16       port,
                                    GCancellable *cancellable)
{
  DexPromise *promise = dex_promise_new_cancellable ();
  g_autoptr (GSocketClient) client = NULL;

  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  g_socket_client_connect_to_host_async (client,
                                         host,
                                         port,
                                         cancellable,
                                         (GAsyncReadyCallback)g_socket_client_connect_to_host_cb,
                                         dex_ref (promise));
  return DEX_FUTURE (g_steal_pointer (&promise));
}

static DexFuture *
handshake_fiber (gpointer user_data)
{
  HandshakeData *data = (HandshakeData *)user_data;
  ValentChannelService *service = data->service;
  GCancellable *cancellable = data->task_cancellable;
  g_autoptr (ValentChannel) channel = NULL;
  g_autoptr (GIOStream) connection = NULL;
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  GTlsCertificate *cert = NULL;
  GTlsCertificate *peer_cert = NULL;
  const char *device_id = NULL;
  int64_t protocol_version = VALENT_NETWORK_PROTOCOL_MAX;
  gboolean is_incoming;
  g_autoptr (GError) error = NULL;

  g_assert (G_IS_IO_STREAM (data->connection) ||
            VALENT_IS_PACKET (data->peer_identity));

  certificate = valent_channel_service_ref_certificate (service);
  identity = valent_channel_service_ref_identity (service);
  is_incoming = (data->connection != NULL);

  /* If this an incoming connection, it is in response to a broadcast from the
   * local device, so the remote device must send its identity.
   *
   * Otherwise, in response to a broadcast from the remote device, the local
   * device must open an outgoing connection and send its identity.
   */
  if (is_incoming)
    {
      g_autoptr (JsonNode) peer_identity = NULL;

      peer_identity = dex_await_boxed (valent_packet_from_stream_future (g_io_stream_get_input_stream (data->connection),
                                                                         IDENTITY_BUFFER_MAX,
                                                                         cancellable),
                                       &error);
      if (peer_identity == NULL)
        goto fail;

      data->peer_identity = g_steal_pointer (&peer_identity);
    }
  else
    {
      data->connection = dex_await_object (_dex_socket_client_connect_to_host (data->host,
                                                                               data->port,
                                                                               cancellable),
                                           &error);
      if (data->connection == NULL)
        goto fail;

      if (!dex_await (valent_packet_to_stream_future (g_io_stream_get_output_stream (data->connection),
                                                      identity,
                                                      cancellable),
                      &error))
        goto fail;
    }

  if (!valent_packet_get_string (data->peer_identity, "deviceId", &device_id))
    {
      g_set_error (&error,
                   VALENT_PACKET_ERROR,
                   VALENT_PACKET_ERROR_MISSING_FIELD,
                   "expected \"deviceId\" field holding a string");
      goto fail;
    }

  if (!valent_device_validate_id (device_id))
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Invalid device ID \"%s\"",
                   device_id);
      goto fail;
    }

  VALENT_JSON (data->peer_identity, data->host);

  /* When negotiating the primary connection, a KDE Connect client acts as the
   * TLS server when opening connections (TCP client) and the TLS client when
   * accepting connections (TCP server).
   */
  connection = dex_await_object (valent_lan_connection_handshake_future (G_SOCKET_CONNECTION (data->connection),
                                                                         certificate,
                                                                         NULL, /* trusted */
                                                                         is_incoming,
                                                                         cancellable),
                                 &error);
  if (connection == NULL)
    goto fail;

  g_clear_object (&data->connection);
  data->connection = g_steal_pointer (&connection);

  if (!valent_packet_get_int (data->peer_identity, "protocolVersion", &protocol_version))
    {
      g_set_error (&error,
                   VALENT_PACKET_ERROR,
                   VALENT_PACKET_ERROR_MISSING_FIELD,
                   "expected \"protocolVersion\" field holding an integer");
      goto fail;
    }

  if (protocol_version > VALENT_NETWORK_PROTOCOL_MAX ||
      protocol_version < VALENT_NETWORK_PROTOCOL_MIN)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_NOT_SUPPORTED,
                   "Unsupported protocol version \"%u\"",
                   (uint8_t) protocol_version);
      goto fail;
    }

  /* In protocol v8, identities are re-exchanged over the encrypted connection
   */
  if (protocol_version >= VALENT_NETWORK_PROTOCOL_V8)
    {
      g_autoptr (JsonNode) secure_identity = NULL;
      DexFuture *send = NULL;
      DexFuture *recv = NULL;
      const char *secure_id = NULL;
      int64_t secure_protocol = VALENT_NETWORK_PROTOCOL_MAX;

      send = valent_packet_to_stream_future (g_io_stream_get_output_stream (data->connection),
                                             identity,
                                             cancellable);
      recv = valent_packet_from_stream_future (g_io_stream_get_input_stream (data->connection),
                                               IDENTITY_BUFFER_MAX,
                                               cancellable);
      if (!dex_await (dex_future_all (send, dex_ref (recv), NULL), &error))
        goto fail;

      secure_identity = dex_await_boxed (recv, &error);
      if (secure_identity == NULL)
        goto fail;

      if (!valent_packet_get_int (secure_identity,
                                  "protocolVersion",
                                  &secure_protocol))
        {
          g_set_error (&error,
                       VALENT_PACKET_ERROR,
                       VALENT_PACKET_ERROR_MISSING_FIELD,
                       "expected \"protocolVersion\" field holding an integer");
          goto fail;
        }

      if (secure_protocol != protocol_version)
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Unexpected protocol version \"%u\"; "
                       "handshake began with version \"%u\"",
                       (uint8_t)secure_protocol,
                       (uint8_t)protocol_version);
          goto fail;
        }

      valent_packet_get_string (data->peer_identity, "deviceId", &device_id);
      if (!valent_packet_get_string (secure_identity, "deviceId", &secure_id))
        {
          g_set_error (&error,
                       VALENT_PACKET_ERROR,
                       VALENT_PACKET_ERROR_MISSING_FIELD,
                       "expected \"deviceId\" field holding a string");
          goto fail;
        }

      if (!g_str_equal (secure_id, device_id))
        {
          g_set_error (&error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Unexpected device ID \"%s\"; "
                       "handshake began with ID \"%s\"",
                       secure_id,
                       device_id);
          goto fail;
        }

      g_clear_pointer (&data->peer_identity, json_node_unref);
      data->peer_identity = g_steal_pointer (&secure_identity);
    }

  cert = g_tls_connection_get_certificate (G_TLS_CONNECTION (data->connection));
  peer_cert = g_tls_connection_get_peer_certificate (G_TLS_CONNECTION (data->connection));
  channel = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                          "base-stream",      data->connection,
                          "certificate",      cert,
                          "identity",         identity,
                          "peer-certificate", peer_cert,
                          "peer-identity",    data->peer_identity,
                          "host",             data->host,
                          "port",             data->port,
                          NULL);
  valent_channel_service_channel (service, channel);
  return dex_future_new_for_boolean (TRUE);

fail:
  if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
  else if (data->cancellable != NULL && !g_cancellable_is_cancelled (data->cancellable))
    g_warning ("%s(): timed out waiting for peer", G_STRFUNC);

  return dex_future_new_for_boolean (FALSE);
}

/*
 * Incoming Connections
 */
static gboolean
on_incoming_connection (ValentChannelService *service,
                        GSocketConnection    *connection,
                        GCancellable         *cancellable,
                        GSocketService       *listener)
{
  DexFuture *handshake = NULL;
  HandshakeData *data = NULL;
  g_autoptr (GSocketAddress) s_addr = NULL;
  GInetAddress *i_addr = NULL;

  g_assert (VALENT_IS_CHANNEL_SERVICE (service));

  s_addr = g_socket_connection_get_remote_address (connection, NULL);
  i_addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (s_addr));

  data = handshake_data_new (service);
  data->connection = g_object_ref (G_IO_STREAM (connection));
  data->host = g_inet_address_to_string (i_addr);
  data->port = VALENT_LAN_PROTOCOL_PORT;

  handshake = dex_scheduler_spawn (dex_scheduler_get_thread_default (),
                                   0, /* stack size */
                                   handshake_fiber,
                                   g_steal_pointer (&data),
                                   handshake_data_free);
  dex_future_disown (g_steal_pointer (&handshake));

  return TRUE;
}

/*
 * Outgoing Connections
 */
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
  GInetAddress *addr = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id;
  g_autofree char *local_id = NULL;
  DexFuture *handshake = NULL;
  HandshakeData *data = NULL;
  g_autoptr (GError) warning = NULL;
  int64_t port = VALENT_LAN_PROTOCOL_PORT;

  VALENT_ENTRY;

  g_assert (G_IS_SOCKET (socket));
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (user_data));

  if (condition != G_IO_IN)
    VALENT_RETURN (G_SOURCE_REMOVE);

  /* Read the message data and extract the remote address
   */
  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (service));
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

      VALENT_RETURN (G_SOURCE_REMOVE);
    }

  if (read == 0)
    {
      g_warning ("%s(): Socket is closed", G_STRFUNC);
      VALENT_RETURN (G_SOURCE_REMOVE);
    }

  /* Validate the message as a KDE Connect packet
   */
  peer_identity = valent_packet_deserialize (buffer, &warning);
  if (peer_identity == NULL)
    {
      g_warning ("%s(): failed to parse peer-identity: %s",
                 G_STRFUNC,
                 warning->message);
      VALENT_RETURN (G_SOURCE_CONTINUE);
    }

  /* Ignore broadcasts without a deviceId or with an invalid deviceId
   */
  if (!valent_packet_get_string (peer_identity, "deviceId", &device_id))
    {
      g_debug ("%s(): expected \"deviceId\" field holding a string",
               G_STRFUNC);
      VALENT_RETURN (G_SOURCE_CONTINUE);
    }

  if (!valent_device_validate_id (device_id))
    {
      g_warning ("%s(): invalid device ID \"%s\"", G_STRFUNC, device_id);
      VALENT_RETURN (G_SOURCE_CONTINUE);
    }

  /* Silently ignore our own broadcasts
   */
  local_id = valent_channel_service_dup_id (service);
  if (g_strcmp0 (device_id, local_id) == 0)
    VALENT_RETURN (G_SOURCE_CONTINUE);

  VALENT_JSON (peer_identity, device_id);

  /* Get the remote address and port
   */
  addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (incoming));
  if (!valent_packet_get_int (peer_identity, "tcpPort", &port) ||
      (port < VALENT_LAN_PROTOCOL_PORT_MIN || port > VALENT_LAN_PROTOCOL_PORT_MAX))
    {
      g_warning ("%s(): expected \"tcpPort\" field holding a uint16 between %u-%u",
                 G_STRFUNC,
                 VALENT_LAN_PROTOCOL_PORT_MIN,
                 VALENT_LAN_PROTOCOL_PORT_MAX);
      VALENT_RETURN (G_SOURCE_CONTINUE);
    }

  data = handshake_data_new (service);
  data->peer_identity = json_node_ref (peer_identity);
  data->host = g_inet_address_to_string (addr);
  data->port = (uint16_t)port;

  handshake = dex_scheduler_spawn (dex_scheduler_get_thread_default (),
                                   0, /* stack size */
                                   handshake_fiber,
                                   g_steal_pointer (&data),
                                   handshake_data_free);
  dex_future_disown (g_steal_pointer (&handshake));

  VALENT_RETURN (G_SOURCE_CONTINUE);
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

  VALENT_ENTRY;

  g_assert (G_IS_SOCKET (socket));
  g_assert (G_IS_SOCKET_ADDRESS (address));

  bytes = g_object_get_data (G_OBJECT (address), "valent-lan-broadcast");
  written = g_socket_send_to (socket,
                              address,
                              g_bytes_get_data (bytes, NULL),
                              g_bytes_get_size (bytes),
                              NULL,
                              &error);

  /* Partial writes are ignored
   */
  if (written == -1)
    {
      g_autofree char *host = NULL;

      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
        VALENT_RETURN (G_SOURCE_CONTINUE);

      host = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (address));
      g_warning ("%s(): failed to announce to \"%s\": %s",
                 G_STRFUNC, host, error->message);
    }

  VALENT_RETURN (G_SOURCE_REMOVE);
}

static gboolean
valent_lan_channel_service_socket_queue (ValentLanChannelService *self,
                                         GSocketAddress          *address)
{
  g_autoptr (GSocket) socket = NULL;
  GSocketFamily family = G_SOCKET_FAMILY_INVALID;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (G_IS_SOCKET_ADDRESS (address));

  family = g_socket_address_get_family (address);
  g_return_val_if_fail (family == G_SOCKET_FAMILY_IPV4 ||
                        family == G_SOCKET_FAMILY_IPV6, FALSE);

  if ((self->udp_socket6 != NULL && family == G_SOCKET_FAMILY_IPV6) ||
      (self->udp_socket6 != NULL && g_socket_speaks_ipv4 (self->udp_socket6)))
    {
      socket = g_object_ref (self->udp_socket6);
    }
  else if (self->udp_socket4 != NULL && family == G_SOCKET_FAMILY_IPV4)
    {
      socket = g_object_ref (self->udp_socket4);
    }

  if (socket != NULL)
    {
      g_autoptr (GSource) source = NULL;
      g_autoptr (JsonNode) identity = NULL;
      g_autoptr (GBytes) identity_bytes = NULL;
      g_autoptr (GCancellable) cancellable = NULL;
      char *identity_json = NULL;
      size_t identity_len;

      /* Embed the serialized identity as private data
       */
      identity = valent_channel_service_ref_identity (VALENT_CHANNEL_SERVICE (self));
      identity_json = valent_packet_serialize (identity, &identity_len);
      identity_bytes = g_bytes_new_take (identity_json, identity_len);
      g_object_set_data_full (G_OBJECT (address),
                              "valent-lan-broadcast",
                              g_bytes_ref (identity_bytes),
                              (GDestroyNotify)g_bytes_unref);

      cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
      source = g_socket_create_source (socket, G_IO_OUT, cancellable);
      g_source_set_callback (source,
                             G_SOURCE_FUNC (valent_lan_channel_service_socket_send),
                             g_object_ref (address),
                             g_object_unref);
      g_source_attach (source, NULL);

      return TRUE;
    }

  return FALSE;
}

static void
g_socket_address_enumerator_next_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GSocketAddressEnumerator *iter = G_SOCKET_ADDRESS_ENUMERATOR (object);
  g_autoptr (ValentLanChannelService) self = g_steal_pointer (&user_data);
  g_autoptr (GSocketAddress) address = NULL;
  g_autoptr (GError) error = NULL;

  address = g_socket_address_enumerator_next_finish (iter, result, &error);
  if (address == NULL)
    {
      if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  if (!valent_lan_channel_service_socket_queue (self, address))
    {
      g_socket_address_enumerator_next_async (iter,
                                              g_task_get_cancellable (G_TASK (result)),
                                              g_socket_address_enumerator_next_cb,
                                              g_object_ref (self));
    }
}

static void
valent_lan_channel_service_socket_queue_resolve (ValentLanChannelService *self,
                                                 GSocketConnectable      *host)
{
  g_autoptr (GSocketAddressEnumerator) iter = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (G_IS_SOCKET_CONNECTABLE (host));

  iter = g_socket_connectable_enumerate (host);
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  g_socket_address_enumerator_next_async (iter,
                                          destroy,
                                          g_socket_address_enumerator_next_cb,
                                          g_object_ref (self));
}

/**
 * valent_lan_channel_service_tcp_setup:
 * @self: a `ValentLanChannelService`
 * @error: (nullable): a `GError`
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

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));

  /* Pass the service as the callback data for the "incoming" signal, while
   * the listener passes the object cancellable as the source object.
   */
  self->listener = g_socket_service_new ();
  g_signal_connect_object (self->listener,
                           "incoming",
                           G_CALLBACK (on_incoming_connection),
                           self,
                           G_CONNECT_SWAPPED);

  self->tcp_port = self->port;
  while (!g_socket_listener_add_inet_port (G_SOCKET_LISTENER (self->listener),
                                           self->tcp_port,
                                           G_OBJECT (destroy),
                                           error))
    {
      if (self->tcp_port >= VALENT_LAN_PROTOCOL_PORT_MAX)
        {
          g_socket_service_stop (self->listener);
          g_socket_listener_close (G_SOCKET_LISTENER (self->listener));
          g_clear_object (&self->listener);

          return FALSE;
        }

      g_clear_error (error);
      self->tcp_port++;
    }

  /* Rebuild the identity packet to populate the `tcpPort` field
   */
  valent_channel_service_build_identity (VALENT_CHANNEL_SERVICE (self));

  return TRUE;
}

/**
 * valent_lan_channel_service_udp_setup:
 * @self: a `ValentLanChannelService`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
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
  g_autoptr (GCancellable) destroy = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (cancellable == NULL || G_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  self->udp_socket6 = g_socket_new (G_SOCKET_FAMILY_IPV6,
                                    G_SOCKET_TYPE_DATAGRAM,
                                    G_SOCKET_PROTOCOL_UDP,
                                    NULL);
  if (self->udp_socket6 != NULL)
    {
      g_autoptr (GInetAddress) inet_address = NULL;
      g_autoptr (GSocketAddress) address = NULL;
      g_autoptr (GSource) source = NULL;

      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV6);
      address = g_inet_socket_address_new (inet_address, self->port);

      if (!g_socket_bind (self->udp_socket6, address, TRUE, NULL))
        {
          g_clear_object (&self->udp_socket6);
          VALENT_GOTO (ipv4);
        }

      g_socket_set_blocking (self->udp_socket6, FALSE);
      g_socket_set_broadcast (self->udp_socket6, TRUE);

      source = g_socket_create_source (self->udp_socket6, G_IO_IN, destroy);
      g_source_set_callback (source,
                             G_SOURCE_FUNC (valent_lan_channel_service_socket_recv),
                             g_object_ref (self),
                             g_object_unref);
      g_source_attach (source, NULL);

      /* If this socket also speaks IPv4, we're done
       */
      if (g_socket_speaks_ipv4 (self->udp_socket6))
        VALENT_RETURN (TRUE);
    }

ipv4:
  self->udp_socket4 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                                    G_SOCKET_TYPE_DATAGRAM,
                                    G_SOCKET_PROTOCOL_UDP,
                                    error);
  if (self->udp_socket4 != NULL)
    {
      g_autoptr (GInetAddress) inet_address = NULL;
      g_autoptr (GSocketAddress) address = NULL;
      g_autoptr (GSource) source = NULL;

      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
      address = g_inet_socket_address_new (inet_address, self->port);

      if (!g_socket_bind (self->udp_socket4, address, TRUE, error))
        {
          g_clear_object (&self->udp_socket4);
          VALENT_GOTO (check);
        }

      g_socket_set_blocking (self->udp_socket4, FALSE);
      g_socket_set_broadcast (self->udp_socket4, TRUE);

      source = g_socket_create_source (self->udp_socket4, G_IO_IN, destroy);
      g_source_set_callback (source,
                             G_SOURCE_FUNC (valent_lan_channel_service_socket_recv),
                             g_object_ref (self),
                             g_object_unref);
      g_source_attach (source, NULL);
    }

check:
  if (self->udp_socket6 == NULL && self->udp_socket4 == NULL)
    VALENT_RETURN (FALSE);

  g_clear_error (error);
  VALENT_RETURN (TRUE);
}

static void
on_items_changed (GListModel              *list,
                  unsigned int             position,
                  unsigned int             removed,
                  unsigned int             added,
                  ValentLanChannelService *self)
{
  g_autofree char *service_id = NULL;

  if (added == 0)
    return;

  service_id = valent_channel_service_dup_id (VALENT_CHANNEL_SERVICE (self));

  for (unsigned int i = 0; i < added; i++)
    {
      g_autoptr (GSocketConnectable) connectable = NULL;
      g_autofree char *device_id = NULL;

      /* Silently ignore our own broadcasts
       */
      connectable = g_list_model_get_item (list, position + i);
      device_id = g_socket_connectable_to_string (connectable);
      if (g_strcmp0 (service_id, device_id) == 0)
        continue;

      if (!g_hash_table_contains (self->channels, device_id))
        valent_lan_channel_service_socket_queue_resolve (self, connectable);
    }
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
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  const char *device_id = NULL;

  g_assert (VALENT_IS_MAIN_THREAD ());
  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));
  g_assert (VALENT_IS_LAN_CHANNEL (channel));

  peer_certificate = valent_channel_ref_peer_certificate (channel);
  device_id = valent_certificate_get_common_name (peer_certificate);

  g_hash_table_replace (self->channels,
                        g_strdup (device_id),
                        g_object_ref (channel));
  g_signal_connect_object (channel,
                           "destroy",
                           G_CALLBACK (on_channel_destroyed),
                           self,
                           G_CONNECT_SWAPPED);
}

static void
valent_lan_channel_service_identify (ValentChannelService *service,
                                     const char           *target)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (service);

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (self));

  if (!self->network_available)
    return;

  if (target != NULL)
    {
      g_autoptr (GSocketConnectable) net = NULL;

      net = g_network_address_parse (target, VALENT_LAN_PROTOCOL_PORT, NULL);
      if (net == NULL)
        {
          g_debug ("%s(): failed to parse \"%s\"", G_STRFUNC, target);
          return;
        }

      valent_lan_channel_service_socket_queue_resolve (self, net);
    }
  else
    {
      g_autoptr (GSocketAddress) address = NULL;
      unsigned int n_items;

      /* Identify to each DNS-SD service
       */
      n_items = g_list_model_get_n_items (self->dnssd);
      for (unsigned int i = 0; i < n_items; i++)
        {
          g_autoptr (GSocketConnectable) item = NULL;

          item = g_list_model_get_item (self->dnssd, i);
          valent_lan_channel_service_socket_queue_resolve (self, item);
        }

      /* Broadcast to the network
       */
      address = g_inet_socket_address_new_from_string (self->broadcast_address,
                                                       self->port);
      valent_lan_channel_service_socket_queue (self, address);
    }
}

/*
 * GInitable
 */
static gboolean
valent_lan_channel_service_init_sync (GInitable     *initable,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (initable);

  g_assert (VALENT_IS_LAN_CHANNEL_SERVICE (initable));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (!valent_lan_channel_service_tcp_setup (self, cancellable, error))
    return FALSE;

  if (!valent_lan_channel_service_udp_setup (self, cancellable, error))
    return FALSE;

  self->dnssd = valent_lan_dnssd_new (NULL);
  g_object_bind_property (self,        "identity",
                          self->dnssd, "identity",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_object (self->dnssd,
                           "items-changed",
                           G_CALLBACK (on_items_changed),
                           self,
                           G_CONNECT_DEFAULT);
  valent_lan_dnssd_start (VALENT_LAN_DNSSD (self->dnssd));

  g_signal_connect_object (self->monitor,
                           "network-changed",
                           G_CALLBACK (on_network_changed),
                           self,
                           G_CONNECT_DEFAULT);
  on_network_changed (self->monitor,
                      g_network_monitor_get_network_available (self->monitor),
                      self);

  return TRUE;
}

static void
g_initable_iface_init (GInitableIface *iface)
{
  iface->init = valent_lan_channel_service_init_sync;
}

/*
 * ValentObject
 */
static void
valent_lan_channel_service_destroy (ValentObject *object)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

  g_signal_handlers_disconnect_by_data (self->monitor, self);

  if (self->dnssd != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->dnssd, self);
      g_clear_object (&self->dnssd);
    }

  g_clear_object (&self->udp_socket4);
  g_clear_object (&self->udp_socket6);

  if (self->listener != NULL)
    {
      g_socket_service_stop (G_SOCKET_SERVICE (self->listener));
      g_socket_listener_close (G_SOCKET_LISTENER (self->listener));
      g_clear_object (&self->listener);
    }

  VALENT_OBJECT_CLASS (valent_lan_channel_service_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_lan_channel_service_finalize (GObject *object)
{
  ValentLanChannelService *self = VALENT_LAN_CHANNEL_SERVICE (object);

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

  switch ((ValentLanChannelServiceProperty)prop_id)
    {
    case PROP_BROADCAST_ADDRESS:
      g_value_set_string (value, self->broadcast_address);
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

  switch ((ValentLanChannelServiceProperty)prop_id)
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
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentChannelServiceClass *service_class = VALENT_CHANNEL_SERVICE_CLASS (klass);

  object_class->finalize = valent_lan_channel_service_finalize;
  object_class->get_property = valent_lan_channel_service_get_property;
  object_class->set_property = valent_lan_channel_service_set_property;

  vobject_class->destroy = valent_lan_channel_service_destroy;

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

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_lan_channel_service_init (ValentLanChannelService *self)
{
  self->channels = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_object_unref);
  self->monitor = g_network_monitor_get_default ();
}

