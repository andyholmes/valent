// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-channel"

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-lan-channel.h"
#include "valent-lan-utils.h"


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
static GIOStream *
valent_lan_channel_download (ValentChannel  *channel,
                             JsonNode       *packet,
                             GCancellable   *cancellable,
                             GError        **error)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (channel);
  JsonObject *info;
  int64_t port;
  goffset size;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  GTlsCertificate *certificate = NULL;
  GTlsCertificate *peer_certificate = NULL;
  g_autofree char *host = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Get the payload information */
  if ((info = valent_packet_get_payload_full (packet, &size, error)) == NULL)
    return NULL;

  if ((port = json_object_get_int_member (info, "port")) == 0 ||
      (port < VALENT_LAN_TRANSFER_PORT_MIN || port > VALENT_LAN_TRANSFER_PORT_MAX))
    {
      g_set_error (error,
                   VALENT_PACKET_ERROR,
                   VALENT_PACKET_ERROR_INVALID_FIELD,
                   "expected \"port\" field holding a uint16 between %u-%u",
                   VALENT_LAN_TRANSFER_PORT_MIN,
                   VALENT_LAN_TRANSFER_PORT_MAX);
      return NULL;
    }

  valent_object_lock (VALENT_OBJECT (self));
  host = g_strdup (self->host);
  valent_object_unlock (VALENT_OBJECT (self));

  /* Open a connection to the host at the expected port
   */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  connection = g_socket_client_connect_to_host (client,
                                                host,
                                                (uint16_t)port,
                                                cancellable,
                                                error);

  if (connection == NULL)
    return NULL;

  /* NOTE: When negotiating an auxiliary connection, a KDE Connect device
   *       acts as the TLS client when opening TCP connections.
   */
  certificate = valent_channel_get_certificate (channel);
  peer_certificate = valent_channel_get_peer_certificate (channel);
  tls_stream = valent_lan_connection_handshake (connection,
                                                certificate,
                                                peer_certificate,
                                                TRUE, /* is_client */
                                                cancellable,
                                                error);
  if (tls_stream == NULL)
    {
      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

static GIOStream *
valent_lan_channel_upload (ValentChannel  *channel,
                           JsonNode       *packet,
                           GCancellable   *cancellable,
                           GError        **error)
{
  JsonObject *info;
  g_autoptr (GSocketListener) listener = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  uint16_t port = VALENT_LAN_TRANSFER_PORT_MIN;
  GTlsCertificate *certificate = NULL;
  GTlsCertificate *peer_certificate = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Find an open port */
  listener = g_socket_listener_new ();

  while (!g_socket_listener_add_inet_port (listener, port, NULL, error))
    {
      if (port >= VALENT_LAN_TRANSFER_PORT_MAX)
        return NULL;

      port++;
      g_clear_error (error);
    }

  /* Set the payload information */
  info = json_object_new();
  json_object_set_int_member (info, "port", (int64_t)port);
  valent_packet_set_payload_info (packet, info);

  /* Notify the device and accept the incoming connection */
  valent_channel_write_packet (channel, packet, cancellable, NULL, NULL);

  connection = g_socket_listener_accept (listener, NULL, cancellable, error);
  g_socket_listener_close (listener);

  if (connection == NULL)
    return NULL;

  /* NOTE: When negotiating an auxiliary connection, a KDE Connect device
   *       acts as the TLS server when accepting TCP connections.
   */
  certificate = valent_channel_get_certificate (channel);
  peer_certificate = valent_channel_get_peer_certificate (channel);
  tls_stream = valent_lan_connection_handshake (connection,
                                                certificate,
                                                peer_certificate,
                                                FALSE, /* is_client */
                                                cancellable,
                                                error);

  if (tls_stream == NULL)
    {
      g_io_stream_close (G_IO_STREAM (connection), NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

/*
 * GObject
 */
static void
valent_lan_channel_finalize (GObject *object)
{
  ValentLanChannel *self = VALENT_LAN_CHANNEL (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_pointer (&self->host, g_free);
  valent_object_unlock (VALENT_OBJECT (self));

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
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_string (value, self->host);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_PORT:
      valent_object_lock (VALENT_OBJECT (self));
      g_value_set_uint (value, self->port);
      valent_object_unlock (VALENT_OBJECT (self));
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
      valent_object_lock (VALENT_OBJECT (self));
      self->host = g_value_dup_string (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_PORT:
      valent_object_lock (VALENT_OBJECT (self));
      self->port = g_value_get_uint (value);
      valent_object_unlock (VALENT_OBJECT (self));
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
   * The remote TCP/IP address for the channel.
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
   * The remote TCP/IP port for the channel.
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

