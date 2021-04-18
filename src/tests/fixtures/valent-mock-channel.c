// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-channel"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-mock-channel.h"

#define VALENT_TEST_TCP_PORT 2716
#define VALENT_TEST_UDP_PORT 2716
#define VALENT_TEST_AUX_MIN  2739
#define VALENT_TEST_AUX_MAX  2764


struct _ValentMockChannel
{
  ValentChannel  parent_instance;

  char          *host;
  guint16        port;
};

G_DEFINE_TYPE (ValentMockChannel, valent_mock_channel, VALENT_TYPE_CHANNEL)

enum {
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * ValentChannel
 */
static const char *
valent_mock_channel_get_verification_key (ValentChannel *channel)
{
  g_assert (VALENT_IS_MOCK_CHANNEL (channel));

  return "Mock Channel";
}

static GIOStream *
valent_mock_channel_download (ValentChannel  *channel,
                              JsonNode       *packet,
                              GCancellable   *cancellable,
                              GError        **error)
{
  ValentMockChannel *self = VALENT_MOCK_CHANNEL (channel);
  JsonObject *info;
  guint16 port;
  gssize size;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Payload Info */
  if ((info = valent_packet_get_payload_full (packet, &size, error)) == NULL)
    return NULL;

  if ((port = valent_packet_check_int (info, "port")) == 0)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_FIELD,
                           "Invalid \"port\" field");
      return NULL;
    }

  /* Wait for connection (open) */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  connection = g_socket_client_connect_to_host (client,
                                                self->host,
                                                port,
                                                cancellable,
                                                error);

  if (connection == NULL)
    return NULL;

  return g_steal_pointer ((GIOStream **)&connection);
}

static GIOStream *
valent_mock_channel_upload (ValentChannel  *channel,
                            JsonNode       *packet,
                            GCancellable   *cancellable,
                            GError        **error)
{
  JsonObject *info;
  g_autoptr (GSocketListener) listener = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  guint16 port;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Wait for an open port */
  listener = g_socket_listener_new ();
  port = VALENT_TEST_AUX_MIN;

  while (port <= VALENT_TEST_AUX_MAX)
    {
      if (g_socket_listener_add_inet_port (listener, port, NULL, error))
        break;
      else if (port < VALENT_TEST_AUX_MAX)
        {
          g_clear_error (error);
          port++;
        }
      else
        return NULL;
    }

  /* Payload Info */
  info = json_object_new();
  json_object_set_int_member (info, "port", (gint64)port);
  valent_packet_set_payload_info (packet, info);

  /* Notify the device we're ready */
  valent_channel_write_packet (channel, packet, cancellable, NULL, NULL);

  /* Wait for connection (accept) */
  connection = g_socket_listener_accept (listener, NULL, cancellable, error);

  if (connection == NULL)
    return NULL;

  return g_steal_pointer ((GIOStream **)&connection);
}

/*
 * GObject
 */
static void
valent_mock_channel_finalize (GObject *object)
{
  ValentMockChannel *self = VALENT_MOCK_CHANNEL (object);

  g_clear_pointer (&self->host, g_free);

  G_OBJECT_CLASS (valent_mock_channel_parent_class)->finalize (object);
}

static void
valent_mock_channel_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentMockChannel *self = VALENT_MOCK_CHANNEL (object);

  switch (prop_id)
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
valent_mock_channel_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentMockChannel *self = VALENT_MOCK_CHANNEL (object);

  switch (prop_id)
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
valent_mock_channel_class_init (ValentMockChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentChannelClass *channel_class = VALENT_CHANNEL_CLASS (klass);

  object_class->finalize = valent_mock_channel_finalize;
  object_class->get_property = valent_mock_channel_get_property;
  object_class->set_property = valent_mock_channel_set_property;

  channel_class->get_verification_key = valent_mock_channel_get_verification_key;
  channel_class->download = valent_mock_channel_download;
  channel_class->upload = valent_mock_channel_upload;

  /**
   * ValentMockChannel:host:
   *
   * The remote TCP/IP address for the channel.
   */
  properties [PROP_HOST] =
    g_param_spec_string ("host",
                         "Host",
                         "TCP/IP address",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMockChannel:port:
   *
   * The remote TCP/IP port for the channel.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port",
                       "Port",
                       "TCP/IP port",
                       0, G_MAXUINT16,
                       VALENT_TEST_TCP_PORT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mock_channel_init (ValentMockChannel *self)
{
  self->host = NULL;
  self->port = VALENT_TEST_TCP_PORT;
}

