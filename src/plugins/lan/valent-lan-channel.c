// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-channel"

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-lan-utils.h"

#include "valent-lan-channel.h"


struct _ValentLanChannel
{
  ValentChannel  parent_instance;

  char          *host;
  uint16_t       port;
};

G_DEFINE_FINAL_TYPE (ValentLanChannel, valent_lan_channel, VALENT_TYPE_CHANNEL)

typedef enum {
  PROP_HOST = 1,
  PROP_PORT,
} ValentLanChannelProperty;

static GParamSpec *properties[PROP_PORT + 1] = { NULL, };


/*
 * ValentChannel
 */
static void
valent_lan_connection_handshake_cb (GSocketConnection *connection,
                                    GAsyncResult      *result,
                                    gpointer           user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (GIOStream) ret = NULL;
  GError *error = NULL;

  ret = valent_lan_connection_handshake_finish (connection, result, &error);
  if (ret == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&ret), g_object_unref);
}

static void
g_socket_client_connect_to_host_cb (GSocketClient *client,
                                    GAsyncResult  *result,
                                    gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentChannel *channel = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  GError *error = NULL;

  connection = g_socket_client_connect_to_host_finish (client, result, &error);
  if (connection == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* NOTE: When negotiating an auxiliary connection, a KDE Connect device
   *       acts as the TLS client when opening TCP connections.
   */
  certificate = valent_channel_ref_certificate (channel);
  peer_certificate = valent_channel_ref_peer_certificate (channel);
  valent_lan_connection_handshake_async (connection,
                                         certificate,
                                         peer_certificate,
                                         TRUE, /* is_client */
                                         cancellable,
                                         (GAsyncReadyCallback)valent_lan_connection_handshake_cb,
                                         g_object_ref (task));
}

static void
valent_lan_channel_download (ValentChannel       *channel,
                             JsonNode            *packet,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (channel);
  g_autoptr (GTask) task = NULL;
  JsonObject *info;
  int64_t port;
  goffset size;
  g_autoptr (GSocketClient) client = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_lan_channel_download);

  /* Get the connection information
   */
  info = valent_packet_get_payload_full (packet, &size, &error);
  if (info == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  if ((port = json_object_get_int_member (info, "port")) == 0 ||
      (port < VALENT_LAN_TRANSFER_PORT_MIN || port > VALENT_LAN_TRANSFER_PORT_MAX))
    {
      g_task_return_new_error (task,
                               VALENT_PACKET_ERROR,
                               VALENT_PACKET_ERROR_INVALID_FIELD,
                               "expected \"port\" field holding a uint16 between %u-%u",
                               VALENT_LAN_TRANSFER_PORT_MIN,
                               VALENT_LAN_TRANSFER_PORT_MAX);
      return;
    }

  /* Open a connection to the host at the expected port
   */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  g_socket_client_connect_to_host_async (client,
                                         self->host,
                                         (uint16_t)port,
                                         cancellable,
                                         (GAsyncReadyCallback)g_socket_client_connect_to_host_cb,
                                         g_object_ref (task));
}

static void
g_socket_listener_accept_cb (GSocketListener *listener,
                             GAsyncResult    *result,
                             gpointer         user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentChannel *channel = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  GError *error = NULL;

  connection = g_socket_listener_accept_finish (listener, result, NULL, &error);
  g_socket_listener_close (listener);
  if (connection == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  /* NOTE: When negotiating an auxiliary connection, a KDE Connect device
   *       acts as the TLS client when opening TCP connections.
   */
  certificate = valent_channel_ref_certificate (channel);
  peer_certificate = valent_channel_ref_peer_certificate (channel);
  valent_lan_connection_handshake_async (connection,
                                         certificate,
                                         peer_certificate,
                                         FALSE, /* is_client */
                                         cancellable,
                                         (GAsyncReadyCallback)valent_lan_connection_handshake_cb,
                                         g_object_ref (task));
}

static void
valent_lan_channel_upload (ValentChannel       *channel,
                           JsonNode            *packet,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GSocketListener) listener = NULL;
  uint16_t port = VALENT_LAN_TRANSFER_PORT_MIN;
  JsonObject *info;
  GError *error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_lan_channel_upload);

  /* Find an open port and prepare the payload information
   */
  listener = g_socket_listener_new ();
  while (!g_socket_listener_add_inet_port (listener, port, NULL, &error))
    {
      if (port >= VALENT_LAN_TRANSFER_PORT_MAX)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      port++;
      g_clear_error (&error);
    }

  info = json_object_new();
  json_object_set_int_member (info, "port", (int64_t)port);
  valent_packet_set_payload_info (packet, info);

  /* Listen for the incoming connection and queue the packet
   */
  g_socket_listener_accept_async (listener,
                                  cancellable,
                                  (GAsyncReadyCallback)g_socket_listener_accept_cb,
                                  g_object_ref (task));
  valent_channel_write_packet (channel, packet, cancellable, NULL, NULL);
}

/*
 * GObject
 */
static void
valent_lan_channel_finalize (GObject *object)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (object);

  g_clear_pointer (&self->host, g_free);

  G_OBJECT_CLASS (valent_lan_channel_parent_class)->finalize (object);
}

static void
valent_lan_channel_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (object);

  switch ((ValentLanChannelProperty)prop_id)
    {
    case PROP_HOST:
      g_value_set_string (value, self->host);
      break;

    case PROP_PORT:
      g_value_set_uint (value, self->port);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_channel_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (object);

  switch ((ValentLanChannelProperty)prop_id)
    {
    case PROP_HOST:
      self->host = g_value_dup_string (value);
      break;

    case PROP_PORT:
      self->port = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_lan_channel_class_init (ValentLanChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelClass *channel_class = VALENT_CHANNEL_CLASS (klass);

  object_class->finalize = valent_lan_channel_finalize;
  object_class->get_property = valent_lan_channel_get_property;
  object_class->set_property = valent_lan_channel_set_property;

  channel_class->download = valent_lan_channel_download;
  channel_class->upload = valent_lan_channel_upload;

  /**
   * ValentLanChannel:host:
   *
   * The remote host for the channel.
   *
   * This could be an IPv4 or IPv6 address, or a hostname.
   */
  properties [PROP_HOST] =
    g_param_spec_string ("host", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentLanChannel:port:
   *
   * The remote port for the channel.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port", NULL, NULL,
                       0, G_MAXUINT16,
                       VALENT_LAN_PROTOCOL_PORT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_lan_channel_init (ValentLanChannel *self)
{
}

