// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-channel"

#include "config.h"

#include <sys/socket.h>

#include <gio/gio.h>
#include <valent.h>

#include "valent-mock-channel.h"

#define VALENT_MOCK_PROTOCOL_PORT_DEFAULT (1717)
#define VALENT_MOCK_PROTOCOL_PORT_MIN     (1717)
#define VALENT_MOCK_PROTOCOL_PORT_MAX     (1764)


struct _ValentMockChannel
{
  ValentChannel  parent_instance;

  char          *host;
  uint16_t       port;
};

G_DEFINE_FINAL_TYPE (ValentMockChannel, valent_mock_channel, VALENT_TYPE_CHANNEL)

typedef enum {
  PROP_HOST = 1,
  PROP_PORT,
} ValentMockChannelProperty;

static GParamSpec *properties[PROP_PORT + 1] = { NULL, };


/*
 * ValentChannel
 */
static void
valent_mock_channel_download (ValentChannel       *channel,
                              JsonNode            *packet,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GSocket) socket = NULL;
  g_autoptr (GIOStream) ret = NULL;
  JsonObject *info;
  int fd = 0;
  goffset size;
  GError *error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  /* Get the fd from the payloadTransferInfo
   */
  info = valent_packet_get_payload_full (packet, &size, &error);
  if (info == NULL)
    {
      g_task_report_error (channel, callback, user_data,
                           valent_mock_channel_download,
                           g_steal_pointer (&error));
      return;
    }

  fd = json_object_get_int_member (info, "fd");
  if (fd == -1)
    {
      g_task_report_new_error (channel, callback, user_data,
                               valent_mock_channel_download,
                               VALENT_PACKET_ERROR,
                               VALENT_PACKET_ERROR_INVALID_FIELD,
                               "expected \"fd\" field holding a file descriptor");
      return;
    }

  socket = g_socket_new_from_fd (fd, &error);
  if (socket == NULL)
    {
      g_task_report_error (channel, callback, user_data,
                           valent_mock_channel_download,
                           g_steal_pointer (&error));
      return;
    }

  ret = g_object_new (G_TYPE_SOCKET_CONNECTION,
                      "socket", socket,
                      NULL);

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_channel_download);
  g_task_return_pointer (task, g_object_ref (ret), g_object_unref);
}

static void
valent_channel_write_packet_cb (ValentChannel *channel,
                                GAsyncResult  *result,
                                gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GSocket *socket = g_task_get_task_data (task);
  g_autoptr (GIOStream) ret = NULL;
  GError *error = NULL;

  if (!valent_channel_write_packet_finish (channel, result, &error))
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  ret = g_object_new (G_TYPE_SOCKET_CONNECTION,
                      "socket", socket,
                      NULL);

  g_task_return_pointer (task, g_object_ref (ret), g_object_unref);
}

static void
valent_mock_channel_upload (ValentChannel       *channel,
                            JsonNode            *packet,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GSocket) socket = NULL;
  JsonObject *info;
  int sv[2] = { 0, };
  GError *error = NULL;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (socketpair (AF_UNIX, SOCK_STREAM, 0, sv) == -1)
    {
      g_task_report_new_error (channel, callback, user_data,
                               valent_mock_channel_upload,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "%s", g_strerror (errno));
      return;
    }

  socket = g_socket_new_from_fd (sv[0], &error);
  if (socket == NULL)
    {
      g_task_report_error (channel, callback, user_data,
                           valent_mock_channel_upload,
                           g_steal_pointer (&error));
      return;
    }

  /* Pass the fd as the payloadTransferInfo
   */
  info = json_object_new();
  json_object_set_int_member (info, "fd", (int64_t)sv[1]);
  valent_packet_set_payload_info (packet, info);

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_channel_upload);
  g_task_set_task_data (task, g_object_ref (socket), g_object_unref);
  valent_channel_write_packet (channel,
                               packet,
                               cancellable,
                               (GAsyncReadyCallback)valent_channel_write_packet_cb,
                               g_object_ref (task));
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

  switch ((ValentMockChannelProperty)prop_id)
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

  switch ((ValentMockChannelProperty)prop_id)
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

  channel_class->download = valent_mock_channel_download;
  channel_class->upload = valent_mock_channel_upload;

  /**
   * ValentMockChannel:host:
   *
   * The remote host address for the channel.
   *
   * This property only exists for tests that require a channel with a `host`
   * property. The underlying connection is actually a `GUnixConnection`.
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
   * property. The underlying connection is actually a `GUnixConnection`.
   */
  properties [PROP_PORT] =
    g_param_spec_uint ("port", NULL, NULL,
                       VALENT_MOCK_PROTOCOL_PORT_MIN, VALENT_MOCK_PROTOCOL_PORT_MAX,
                       VALENT_MOCK_PROTOCOL_PORT_DEFAULT,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_mock_channel_init (ValentMockChannel *self)
{
}

