// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-channel"

#include "config.h"

#include <sys/socket.h>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-mock-channel.h"

#define VALENT_MOCK_PROTOCOL_PORT_DEFAULT (1717)
#define VALENT_MOCK_PROTOCOL_PORT_MIN     (1717)
#define VALENT_MOCK_PROTOCOL_PORT_MAX     (1764)


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


/* HACK: ThreadSanitizer reports a data race in socketpair() */
static GRecMutex socketpair_lock;


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
  g_autoptr (GSocket) socket = NULL;
  JsonObject *info;
  int fd = 0;
  goffset size;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Payload Info */
  if ((info = valent_packet_get_payload_full (packet, &size, error)) == NULL)
    return NULL;

  if ((fd = json_object_get_int_member (info, "fd")) == -1)
    {
      g_set_error_literal (error,
                           VALENT_PACKET_ERROR,
                           VALENT_PACKET_ERROR_INVALID_FIELD,
                           "expected \"fd\" field holding a file descriptor");
      return NULL;
    }

  if ((socket = g_socket_new_from_fd (fd, error)) == NULL)
    return NULL;

  /* Send a single byte to confirm connection */
  if (g_socket_send (socket, "\x06", 1, cancellable, error) == -1)
    return NULL;

  return g_object_new (G_TYPE_SOCKET_CONNECTION,
                       "socket", socket,
                       NULL);
}

static GIOStream *
valent_mock_channel_upload (ValentChannel  *channel,
                            JsonNode       *packet,
                            GCancellable   *cancellable,
                            GError        **error)
{
  JsonObject *info;
  g_autoptr (GSocket) socket = NULL;
  int sv[2] = { 0, };
  char buf[1] = { 0, };

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  g_rec_mutex_lock (&socketpair_lock);
  if (socketpair (AF_UNIX, SOCK_STREAM, 0, sv) == -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           g_strerror (errno));
      g_rec_mutex_unlock (&socketpair_lock);
      return NULL;
    }
  g_rec_mutex_unlock (&socketpair_lock);

  /* Payload Info */
  info = json_object_new();
  json_object_set_int_member (info, "fd", (gint64)sv[1]);
  valent_packet_set_payload_info (packet, info);

  if ((socket = g_socket_new_from_fd (sv[0], error)) == NULL)
    return NULL;

  /* Notify the device we're ready */
  valent_channel_write_packet (channel, packet, cancellable, NULL, NULL);

  /* Receive a single byte to confirm connection */
  if (g_socket_receive (socket, buf, sizeof (buf), cancellable, error) == -1)
    return NULL;

  return g_object_new (G_TYPE_SOCKET_CONNECTION,
                       "socket", socket,
                       NULL);
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
   * The remote host address for the channel.
   *
   * This property only exists for tests that require a channel with a `host`
   * property. The underlying connection is actually a #GUnixConnection.
   */
  properties [PROP_HOST] =
    g_param_spec_string ("host", NULL, NULL,
                         "127.0.0.1",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMockChannel:port:
   *
   * The remote host port for the channel.
   *
   * This property only exists for tests that require a channel with a `port`
   * property. The underlying connection is actually a #GUnixConnection.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port", NULL, NULL,
                       VALENT_MOCK_PROTOCOL_PORT_MIN, VALENT_MOCK_PROTOCOL_PORT_MAX,
                       VALENT_MOCK_PROTOCOL_PORT_DEFAULT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mock_channel_init (ValentMockChannel *self)
{
}

