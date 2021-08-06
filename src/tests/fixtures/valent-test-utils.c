// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>
#include <stdio.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <json-glib/json-glib.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-mock-channel.h"
#include "valent-test-utils.h"


/*
 * Channel Helpers
 */
typedef struct
{
  ValentChannel *channel;
  JsonNode      *channel_identity;
  ValentChannel *endpoint;
  JsonNode      *endpoint_identity;
  guint16        port;
} ChannelData;

static void
accept_cb (GSocketListener *listener,
           GAsyncResult    *result,
           gpointer         user_data)
{
  ChannelData *data = user_data;
  g_autoptr (GSocketConnection) base_stream = NULL;

  base_stream = g_socket_listener_accept_finish (listener, result, NULL, NULL);
  data->channel = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                                "base-stream",   base_stream,
                                "host",          "127.0.0.1",
                                "identity",      data->channel_identity,
                                "peer-identity", data->endpoint_identity,
                                "port",          data->port,
                                NULL);
}

static void
connect_cb (GSocketClient *client,
            GAsyncResult  *result,
            gpointer       user_data)
{
  ChannelData *data = user_data;
  g_autoptr (GSocketConnection) base_stream = NULL;

  base_stream = g_socket_client_connect_finish (client, result, NULL);
  data->endpoint = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                                 "base-stream",   base_stream,
                                 "host",          "127.0.0.1",
                                 "identity",      data->endpoint_identity,
                                 "peer-identity", data->channel_identity,
                                 "port",          data->port,
                                 NULL);
}

/*
 * Transfer Helpers
 */
typedef struct
{
  GMainLoop *loop;
  JsonNode  *packet;
  GFile     *file;
  gboolean   success;
  GError    *error;
} TransferOperation;

static void
transfer_op_free (gpointer data)
{
  TransferOperation *op = data;

  g_clear_object (&op->file);
  g_clear_pointer (&op->loop, g_main_loop_unref);
  g_clear_pointer (&op->packet, json_node_unref);
  g_free (op);
}

static void
transfer_cb (GObject      *object,
             GAsyncResult *result,
             gpointer      user_data)
{
  TransferOperation *op;

  op = g_task_get_task_data (G_TASK (result));
  op->success = g_task_propagate_boolean (G_TASK (result), &op->error);

  g_main_loop_quit (op->loop);
}

static void
upload_task (GTask        *task,
             gpointer      source_object,
             gpointer      task_data,
             GCancellable *cancellable)
{
  TransferOperation *op = task_data;
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GFileInfo) file_info = NULL;
  g_autoptr (GFileInputStream) file_source = NULL;
  gssize size;
  GError *error = NULL;

  file_info = g_file_query_info (op->file, "standard::size", 0, NULL, &error);

  if (file_info == NULL)
    return g_task_return_error (task, error);

  file_source = g_file_read (op->file, cancellable, &error);

  if (file_source == NULL)
    return g_task_return_error (task, error);

  size = g_file_info_get_size (file_info);
  valent_packet_set_payload_size (op->packet, size);

  stream = valent_channel_upload (VALENT_CHANNEL (source_object),
                                  op->packet,
                                  cancellable,
                                  &error);

  if (stream == NULL)
    return g_task_return_error (task, error);

  g_output_stream_splice (g_io_stream_get_output_stream (stream),
                          G_INPUT_STREAM (file_source),
                          (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                           G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                          cancellable,
                          &error);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
download_task (GTask        *task,
               gpointer      source_object,
               gpointer      task_data,
               GCancellable *cancellable)
{
  TransferOperation *op = task_data;
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GOutputStream) target = NULL;
  GError *error = NULL;

  stream = valent_channel_download (VALENT_CHANNEL (source_object),
                                    op->packet,
                                    cancellable,
                                    &error);

  if (stream == NULL)
    return g_task_return_error (task, error);

  target = g_memory_output_stream_new_resizable ();

  g_output_stream_splice (target,
                          g_io_stream_get_input_stream (stream),
                          (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                           G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                          cancellable,
                          &error);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}


/**
 * valent_test_mute_domain:
 * @log_domain: the log domain of the message
 * @log_level:
 * @message:
 * @user_data: the log domain to match against
 *
 * A #GTestLogFatalFunc for preventing fatal errors if @log_domain matches
 * @user_data with g_strcmp0().
 */
gboolean
valent_test_mute_domain (const char     *log_domain,
                         GLogLevelFlags  log_level,
                         const char     *message,
                         gpointer        user_data)
{
  const char *match_domain = user_data;

  if (g_strcmp0 (log_domain, match_domain) == 0)
    return FALSE;

  return TRUE;
}

/**
 * valent_test_mute_match:
 * @log_domain: the log domain of the message
 * @log_level:
 * @message:
 * @user_data: the log domain to match against
 *
 * A #GTestLogFatalFunc for preventing fatal errors if @message matches
 * @user_data with g_match_regex_simple().
 */
gboolean
valent_test_mute_match (const char     *log_domain,
                        GLogLevelFlags  log_level,
                        const char     *message,
                        gpointer        user_data)
{
  if (g_regex_match_simple (user_data, message, 0, 0) ||
      g_pattern_match_simple (user_data, message))
    return FALSE;

  return TRUE;
}

/**
 * valent_test_mute_warning:
 * @log_domain: the log domain of the message
 * @log_level:
 * @message:
 * @user_data: the log domain to match against
 *
 * A #GTestLogFatalFunc for preventing warnings from aborting a test.
 */
gboolean
valent_test_mute_warning (const char     *log_domain,
                          GLogLevelFlags  log_level,
                          const char     *message,
                          gpointer        user_data)
{
  if (log_level & G_LOG_LEVEL_WARNING)
    return FALSE;

  return TRUE;
}

/**
 * valent_test_mute_fuzzing:
 * @log_domain: the log domain of the message
 * @log_level: the log level of the message
 * @message: the message to process
 * @user_data: the log domain to match against
 *
 * A #GTestLogFatalFunc for quieting fatal logging during fuzzing tests. This
 * includes messages:
 *
 * - @log_domain is `Json`
 * - @log_level is %G_LOG_LEVEL_WARNING
 *
 * Returns: %TRUE if not matching
 */
gboolean
valent_test_mute_fuzzing (const char     *log_domain,
                          GLogLevelFlags  log_level,
                          const char     *message,
                          gpointer        user_data)
{
  if (log_level & G_LOG_LEVEL_WARNING)
    return FALSE;

  if (g_strcmp0 (log_domain, "Json") == 0)
    return FALSE;

  return TRUE;
}

static GQueue *events = NULL;

/**
 * valent_test_event_free:
 * @free_func: the function to be called to free each event item
 *
 * Clear the event queue.
 */
void
valent_test_event_free (GDestroyNotify free_func)
{
  if (events == NULL)
    return;

  g_queue_free_full (events, free_func);
  events = NULL;
}

/**
 * valent_test_event_pop:
 *
 * Remove and return the event at the head of the queue
 *
 * Returns: (transfer full): an event
 */
gpointer
valent_test_event_pop (void)
{
  if (events == NULL)
    return NULL;

  return g_queue_pop_head (events);
}

/**
 * valent_test_event_push:
 * @event: an event
 *
 * Add @event to the end of the event queue.
 */
void
valent_test_event_push (gpointer event)
{
  if (events == NULL)
    events = g_queue_new ();

  g_queue_push_tail (events, event);
}

JsonNode *
valent_test_load_json (const char *path)
{
  g_autoptr (JsonParser) parser = NULL;

  parser = json_parser_new ();
  json_parser_load_from_file (parser, path, NULL);

  return json_parser_steal_root (parser);
}

/**
 * valent_test_channels:
 * @identity: a #JsonNode
 * @peer_identity: (nullable): a #JsonNode
 * @port: the local port
 *
 * Create a pair of connected channels with @identity representing the local
 * device and @peer_identity representing the endpoint device.
 *
 * Returns: (array length=2) (element-type Valent.Channel): a pair of #ValentChannel
 */
ValentChannel **
valent_test_channels (JsonNode *identity,
                      JsonNode *peer_identity)
{
  g_autofree ChannelData *data = NULL;
  g_autoptr (GSocketListener) listener = NULL;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketAddress) addr = NULL;
  ValentChannel **channels = NULL;

  g_assert (VALENT_IS_PACKET (identity));
  g_assert (peer_identity == NULL || VALENT_IS_PACKET (peer_identity));

  data = g_new0 (ChannelData, 1);
  data->channel_identity = identity;
  data->endpoint_identity = identity;
  data->port = 2716;

  /* Wait for incoming connection */
  listener = g_socket_listener_new ();

  while (!g_socket_listener_add_inet_port (listener, data->port, NULL, NULL))
    data->port++;

  g_socket_listener_accept_async (listener,
                                  NULL,
                                  (GAsyncReadyCallback)accept_cb,
                                  data);

  /* Open outgoing connection */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  addr = g_inet_socket_address_new_from_string ("127.0.0.1", data->port);

  g_socket_client_connect_async (client,
                                 G_SOCKET_CONNECTABLE (addr),
                                 NULL,
                                 (GAsyncReadyCallback)connect_cb,
                                 data);

  /* Wait for both channels */
  while (data->channel == NULL || data->endpoint == NULL)
    g_main_context_iteration (NULL, FALSE);

  channels = g_new0 (ValentChannel *, 2);
  channels[0] = g_steal_pointer (&data->channel);
  channels[1] = g_steal_pointer (&data->endpoint);

  return channels;
}

/**
 * valent_test_download:
 * @channel: a #ValentChannel
 * @packet: a #JsonNode
 * @error: (nullable): a #GError
 *
 * Simulate downloading the transfer described by @packet from the endpoint of @channel.
 *
 * Returns: %TRUE if successful
 */
gboolean
valent_test_download (ValentChannel  *channel,
                      JsonNode       *packet,
                      GError        **error)
{
  g_autoptr (GTask) task = NULL;
  TransferOperation *op;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (error == NULL || *error == NULL);

  op = g_new0 (TransferOperation, 1);
  op->loop = g_main_loop_new (NULL, FALSE);
  op->packet = json_node_ref (packet);

  task = g_task_new (channel, NULL, transfer_cb, NULL);
  g_task_set_task_data (task, op, transfer_op_free);
  g_task_run_in_thread (task, download_task);
  g_main_loop_run (op->loop);

  if (op->error != NULL)
    g_propagate_error (error, op->error);

  return op->success;
}

/**
 * valent_test_upload:
 * @channel: a #ValentChannel
 * @packet: a #JsonNode
 * @file: a #GFile
 * @error: (nullable): a #GError
 *
 * Simulate uploading @file to the endpoint of @channel.
 */
gboolean
valent_test_upload (ValentChannel  *channel,
                    JsonNode       *packet,
                    GFile          *file,
                    GError        **error)
{
  g_autoptr (GTask) task = NULL;
  TransferOperation *op;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (G_IS_FILE (file));
  g_assert (error == NULL || *error == NULL);

  op = g_new0 (TransferOperation, 1);
  op->loop = g_main_loop_new (NULL, FALSE);
  op->packet = json_node_ref (packet);
  op->file = g_object_ref (file);

  task = g_task_new (channel, NULL, transfer_cb, NULL);
  g_task_set_task_data (task, op, transfer_op_free);
  g_task_run_in_thread (task, upload_task);
  g_main_loop_run (op->loop);

  return op->success;
}

/**
 * valent_test_ui_init:
 * @argcp: Address of the `argc` parameter of the
 *        main() function. Changed if any arguments were handled.
 * @argvp: (inout) (array length=argcp): Address of the
 *        `argv` parameter of main().
 *        Any parameters understood by g_test_init() or gtk_init() are
 *        stripped before return.
 * @...: currently unused
 *
 * This function is used to initialize a GUI test program for Valent.
 *
 * In order, it will:
 * - Call g_test_init() passing @argcp, @argvp and %G_TEST_OPTION_ISOLATE_DIRS
 * - Set the locale to “en_US.UTF-8”
 * - Call gtk_init()
 * - Call adw_init()
 *
 * Like g_test_init(), any known arguments will be processed and stripped from
 * @argcp and @argvp.
 */
void
valent_test_ui_init (int    *argcp,
                     char ***argvp,
                     ...)
{
  g_test_init (argcp, argvp, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  gtk_disable_setlocale ();
  setlocale (LC_ALL, "en_US.UTF-8");

  gtk_init ();
  adw_init ();
}

