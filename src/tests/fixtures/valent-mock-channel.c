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
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GIOStream) stream = NULL;
  g_autofree char *host = NULL;
  JsonObject *info;
  guint16 port;
  gssize size;

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
  host = valent_mock_channel_dup_host (self);
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  connection = g_socket_client_connect_to_host (client,
                                                host,
                                                port,
                                                cancellable,
                                                error);

  if (connection != NULL)
    {
      stream = G_IO_STREAM (connection);
      connection = NULL;
    }

  return g_steal_pointer (&stream);
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
  g_autoptr (GIOStream) stream = NULL;
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

  if (connection != NULL)
    {
      stream = G_IO_STREAM (connection);
      connection = NULL;
    }

  return g_steal_pointer (&stream);
}

/*
 * GObject
 */
static void
valent_mock_channel_finalize (GObject *object)
{
  ValentMockChannel *self = VALENT_MOCK_CHANNEL (object);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_pointer (&self->host, g_free);
  valent_object_unlock (VALENT_OBJECT (self));

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
      g_value_take_string (value, valent_mock_channel_dup_host (self));
      break;

    case PROP_PORT:
      g_value_set_uint (value, valent_mock_channel_get_port (self));
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

/**
 * valent_mock_channel_dup_host:
 * @self: a #ValentMockChannel
 *
 * Get the host or IP address for @self.
 *
 * Returns: (transfer full) (nullable): a host or IP address
 */
char *
valent_mock_channel_dup_host (ValentMockChannel *self)
{
  char *ret;

  g_return_val_if_fail (VALENT_IS_MOCK_CHANNEL (self), NULL);

  valent_object_lock (VALENT_OBJECT (self));
  ret = g_strdup (self->host);
  valent_object_unlock (VALENT_OBJECT (self));

  return ret;
}

/**
 * valent_mock_channel_get_port:
 * @self: a #ValentMockChannel
 *
 * Get the port for @self.
 *
 * Returns: (transfer full) (nullable): a port number
 */
guint16
valent_mock_channel_get_port (ValentMockChannel *self)
{
  guint16 ret;

  g_return_val_if_fail (VALENT_IS_MOCK_CHANNEL (self), 0);

  valent_object_lock (VALENT_OBJECT (self));
  ret = self->port;
  valent_object_unlock (VALENT_OBJECT (self));

  return ret;
}

