// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <locale.h>
#include <stdio.h>
#include <sys/socket.h>

#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-mock-channel.h"
#include "valent-mock-network-monitor.h"

#include "valent-test-utils.h"

/**
 * valent_test_init:
 * @argcp: Address of the `argc` parameter of the
 *        main() function. Changed if any arguments were handled.
 * @argvp: (inout) (array length=argcp): Address of the
 *        `argv` parameter of main().
 *        Any parameters understood by g_test_init() or gtk_init() are
 *        stripped before return.
 * @...: currently unused
 *
 * This function is used to initialize a test program for Valent.
 *
 * In order, it will:
 * - Call g_content_type_set_mime_dirs() to ensure GdkPixbuf works
 * - Call g_test_init() with the %G_TEST_OPTION_ISOLATE_DIRS option
 * - Call g_type_ensure() for all public classes
 *
 * Like g_test_init(), any known arguments will be processed and stripped from
 * @argcp and @argvp.
 */
void
valent_test_init (int    *argcp,
                  char ***argvp,
                  ...)
{
  GIOExtensionPoint *ep;

  g_content_type_set_mime_dirs (NULL);
  g_test_init (argcp, argvp, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  /* GIO extensions
   */
  ep = g_io_extension_point_register (G_NETWORK_MONITOR_EXTENSION_POINT_NAME);
  g_io_extension_point_set_required_type (ep, G_TYPE_NETWORK_MONITOR);
  g_type_ensure (VALENT_TYPE_MOCK_NETWORK_MONITOR);

  /* Core */
  g_type_ensure (VALENT_TYPE_APPLICATION);
  g_type_ensure (VALENT_TYPE_APPLICATION_PLUGIN);
  g_type_ensure (VALENT_TYPE_CONTEXT);
  g_type_ensure (VALENT_TYPE_EXTENSION);
  g_type_ensure (VALENT_TYPE_OBJECT);
  g_type_ensure (VALENT_TYPE_COMPONENT);
  g_type_ensure (VALENT_TYPE_TRANSFER);

  /* Components */
  g_type_ensure (VALENT_TYPE_CLIPBOARD);
  g_type_ensure (VALENT_TYPE_CLIPBOARD_ADAPTER);
  g_type_ensure (VALENT_TYPE_CONTACTS);
  g_type_ensure (VALENT_TYPE_CONTACTS_ADAPTER);
  g_type_ensure (VALENT_TYPE_INPUT);
  g_type_ensure (VALENT_TYPE_INPUT_ADAPTER);
  g_type_ensure (VALENT_TYPE_MEDIA);
  g_type_ensure (VALENT_TYPE_MEDIA_ADAPTER);
  g_type_ensure (VALENT_TYPE_MEDIA_PLAYER);
  g_type_ensure (VALENT_TYPE_MESSAGES);
  g_type_ensure (VALENT_TYPE_MESSAGES_ADAPTER);
  g_type_ensure (VALENT_TYPE_MESSAGE);
  g_type_ensure (VALENT_TYPE_MESSAGE_ATTACHMENT);
  g_type_ensure (VALENT_TYPE_MIXER);
  g_type_ensure (VALENT_TYPE_MIXER_ADAPTER);
  g_type_ensure (VALENT_TYPE_MIXER_STREAM);
  g_type_ensure (VALENT_TYPE_NOTIFICATIONS);
  g_type_ensure (VALENT_TYPE_NOTIFICATIONS_ADAPTER);
  g_type_ensure (VALENT_TYPE_NOTIFICATION);
  g_type_ensure (VALENT_TYPE_SESSION);
  g_type_ensure (VALENT_TYPE_SESSION_ADAPTER);

  /* Device */
  g_type_ensure (VALENT_TYPE_CHANNEL);
  g_type_ensure (VALENT_TYPE_CHANNEL_SERVICE);
  g_type_ensure (VALENT_TYPE_DEVICE);
  g_type_ensure (VALENT_TYPE_DEVICE_MANAGER);
  g_type_ensure (VALENT_TYPE_DEVICE_PLUGIN);
  g_type_ensure (VALENT_TYPE_DEVICE_TRANSFER);
}

static GMainLoop *test_loop = NULL;

/**
 * valent_test_run_loop:
 *
 * Run the default main loop.
 */
void
valent_test_run_loop (void)
{
  if (test_loop == NULL)
    test_loop = g_main_loop_new (NULL, FALSE);

  g_main_loop_run (test_loop);
}

/**
 * valent_test_quit_loop:
 *
 * Run the default main loop.
 */
gboolean
valent_test_quit_loop (void)
{
  g_assert (test_loop != NULL);

  g_main_loop_quit (test_loop);
  return G_SOURCE_REMOVE;
}

/**
 * valent_test_mute_fuzzing:
 * @log_domain: the log domain of the message
 * @log_level: the log level of the message
 * @message: the message to process
 * @user_data: the log domain to match against
 *
 * A `GTestLogFatalFunc` for quieting fatal logging during fuzzing tests. This
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

/**
 * valent_test_load_json:
 * @path: (type filename): path to JSON file
 *
 * A simple convenience to load the JSON at @path.
 *
 * Returns: (transfer full): a `JsonNode`
 */
JsonNode *
valent_test_load_json (const char *path)
{
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *resource_path = NULL;
  GError *error = NULL;

  resource_path = g_build_filename ("/tests", path, NULL);
  bytes = g_resources_lookup_data (resource_path, 0, &error);
  g_assert_no_error (error);

  parser = json_parser_new ();
  json_parser_load_from_data (parser, g_bytes_get_data (bytes, NULL), -1, &error);
  g_assert_no_error (error);

  return json_parser_steal_root (parser);
}

/**
 * valent_test_mock_settings:
 * @context: a context path
 * @module_name: a `PeasPluginInfo` module name
 *
 * A convenience function to create a `GSettings` object for component domain.
 *
 * Returns: (transfer full): the new `GSettings` object
 */
GSettings *
valent_test_mock_settings (const char *domain)
{
  g_autofree char *path = NULL;

  g_assert (domain != NULL && *domain != '\0');

  path = g_strdup_printf ("/ca/andyholmes/valent/%s/plugin/mock/", domain);

  return g_settings_new_with_path ("ca.andyholmes.Valent.Plugin", path);
}

/**
 * valent_test_await_adapter:
 * @component: (type Valent.Component): a `ValentComponent`
 *
 * Wait for a [class@Valent.Component] adapter to load and return it.
 *
 * Returns: a component adapter
 */
gpointer
valent_test_await_adapter (gpointer component)
{
  gpointer ret = NULL;

  g_assert (VALENT_IS_COMPONENT (component));

  while ((ret = valent_component_get_primary_adapter (component)) == NULL)
    g_main_context_iteration (NULL, FALSE);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  return ret;
}

static gboolean
valent_test_await_boolean_cb (gpointer data)
{
  gboolean *done = (gboolean *)data;

  if (*done == FALSE)
    return G_SOURCE_CONTINUE;

  *done = FALSE;
  return valent_test_quit_loop ();
}

/**
 * valent_test_await_boolean:
 * @done: a pointer to a `gboolean`
 *
 * Wait for @done to be set to %TRUE, while iterating the main context.
 *
 * This function will reset @done to %FALSE, before returning.
 *
 * This is useful for iterating the main context until an asynchronous operation
 * completes, rather than running a loop.
 */
void
(valent_test_await_boolean) (gboolean *done)
{
  g_assert (done != NULL);

  if G_UNLIKELY (*done == TRUE)
    {
      *done = FALSE;
      return;
    }

  g_idle_add_full (INT_MAX, valent_test_await_boolean_cb, done, NULL);
  valent_test_run_loop ();
}

/**
 * valent_test_await_pending:
 *
 * Wait for any pending sources to dispatch.
 *
 * This is useful for iterating the main context until known operations complete
 * that can't be introspected by tasks (i.e. internal asynchronous operations).
 */
void
(valent_test_await_pending) (void)
{
  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

static gboolean
valent_test_await_pointer_cb (gpointer data)
{
  gpointer *target = (gpointer *)data;

  if (*target == NULL)
    return G_SOURCE_CONTINUE;

  return valent_test_quit_loop ();
}

/**
 * valent_test_await_pointer:
 * @result: a pointer to a `gpointer` (i.e. `void **`)
 *
 * Wait for @ptr to be changed from %NULL, while iterating the main context.
 *
 * This is useful for iterating the main context until an asynchronous operation
 * completes, rather than running a loop.
 */
void
(valent_test_await_pointer) (gpointer *result)
{
  g_assert (result != NULL);

  if G_UNLIKELY (*result != NULL)
    return;

  g_idle_add_full (INT_MAX, valent_test_await_pointer_cb, result, NULL);
  valent_test_run_loop ();
}

static gboolean
valent_test_await_nullptr_cb (gpointer data)
{
  gpointer *target = (gpointer *)data;

  if (*target != NULL)
    return G_SOURCE_CONTINUE;

  return valent_test_quit_loop ();
}

/**
 * valent_test_await_nullptr:
 * @result: a pointer to a `gpointer` (i.e. `void **`)
 *
 * Wait for @ptr to be changed to %NULL, while iterating the main context.
 *
 * This is useful for iterating the main context until an asynchronous operation
 * completes, rather than running a loop.
 */
void
(valent_test_await_nullptr) (gpointer *result)
{
  g_assert (result != NULL);

  if G_UNLIKELY (*result == NULL)
    return;

  g_idle_add_full (INT_MAX, valent_test_await_nullptr_cb, result, NULL);
  valent_test_run_loop ();
}

/**
 * valent_test_await_signal:
 * @object: (type GObject.Object): a `GObject`
 * @signal_name: a signal to wait for
 *
 * Wait for @object to emit @signal_name, while iterating the main context.
 */
void
valent_test_await_signal (gpointer    object,
                          const char *signal_name)
{
  gulong handler_id = 0;

  handler_id = g_signal_connect_swapped (G_OBJECT (object),
                                         signal_name,
                                         G_CALLBACK (valent_test_quit_loop),
                                         NULL);
  valent_test_run_loop ();
  g_clear_signal_handler (&handler_id, G_OBJECT (object));
}

static void
valent_test_watch_signal_cb (gpointer data)
{
  gboolean *done = data;

  if (done != NULL)
    *done = TRUE;
}

/**
 * valent_test_watch_signal:
 * @object: (type GObject.Object): a `GObject`
 * @signal_name: a signal to wait for
 * @watch: a pointer to a `gboolean`
 *
 * Watch for @object to emit @signal_name, then set @watch to %TRUE.
 *
 * Call [func@Valent.test_await_boolean] to wait for the emission, and
 * [func@Valent.test_unwatch_signal] to remove all signals for @watch.
 */
void
valent_test_watch_signal (gpointer    object,
                          const char *signal_name,
                          gboolean   *watch)
{
  g_signal_connect_swapped (object,
                            signal_name,
                            G_CALLBACK (valent_test_watch_signal_cb),
                            watch);
}

/**
 * valent_test_watch_clear:
 * @object: (type GObject.Object): a `GObject`
 * @watch: a pointer to a `gboolean`
 *
 * Remove all signal handlers on @object for @watch.
 */
void
valent_test_watch_clear (gpointer  object,
                         gboolean *watch)
{
  g_signal_handlers_disconnect_by_func (object,
                                        valent_test_watch_signal_cb,
                                        watch);
}

/**
 * valent_test_await_timeout:
 * @duration: the time to wait, in milliseconds
 *
 * Iterate the default main context for @duration.
 */
void
valent_test_await_timeout (unsigned int duration)
{
  g_timeout_add (duration, G_SOURCE_FUNC (valent_test_quit_loop), NULL);
  valent_test_run_loop ();
}

/**
 * valent_test_channel_pair:
 * @identity: a KDE Connect identity packet for the remote device
 * @peer_identity: a KDE Connect identity packet for the host system
 * @channel_out: (not nullable) (out): a location for a `ValentChannel`
 * @peer_channel_out: (not nullable) (out): a location for a `ValentChannel`
 *
 * Create a pair of connected channels with @identity representing the local
 * device and @peer_identity representing the endpoint device.
 */
void
valent_test_channel_pair (JsonNode       *identity,
                          JsonNode       *peer_identity,
                          ValentChannel **channel_out,
                          ValentChannel **peer_channel_out)
{
  int sv[2] = { 0, };
  g_autoptr (GSocket) socket = NULL;
  g_autoptr (GSocket) peer_socket = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GSocketConnection) peer_connection = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_PACKET (identity));
  g_assert (VALENT_IS_PACKET (peer_identity));
  g_assert (channel_out != NULL && peer_channel_out != NULL);

  /* Generate certificates and update the identity packets
   */
  certificate = valent_certificate_new_sync (NULL, &error);
  g_assert_no_error (error);
  json_object_set_string_member (valent_packet_get_body (identity),
                                 "deviceId",
                                 valent_certificate_get_common_name (certificate));

  peer_certificate = valent_certificate_new_sync (NULL, &error);
  g_assert_no_error (error);
  json_object_set_string_member (valent_packet_get_body (peer_identity),
                                 "deviceId",
                                 valent_certificate_get_common_name (peer_certificate));

  /* Open a socket pair to back the connections
   */
  g_assert_no_errno (socketpair (AF_UNIX, SOCK_STREAM, 0, sv));

  /* This is the channel associated with the ValentDevice object
   */
  socket = g_socket_new_from_fd (sv[0], &error);
  g_assert_no_error (error);
  connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                             "socket", socket,
                             NULL);
  *channel_out = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                               "base-stream",      connection,
                               "certificate",      certificate,
                               "identity",         identity,
                               "peer-certificate", peer_certificate,
                               "peer-identity",    peer_identity,
                               NULL);

  /* This is the channel associated with the mock endpoint
   */
  peer_socket = g_socket_new_from_fd (sv[1], &error);
  g_assert_no_error (error);
  peer_connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                                  "socket", peer_socket,
                                  NULL);
  *peer_channel_out = g_object_new (VALENT_TYPE_MOCK_CHANNEL,
                                    "base-stream",      peer_connection,
                                    "certificate",      peer_certificate,
                                    "identity",         peer_identity,
                                    "peer-certificate", certificate,
                                    "peer-identity",    identity,
                                    NULL);
}

typedef struct
{
  GIOStream    *stream;
  JsonNode     *packet;
  GInputStream *source;
} TransferOperation;

static void
transfer_operation_free (gpointer data)
{
  TransferOperation *operation = (TransferOperation *)data;

  g_clear_object (&operation->stream);
  g_clear_object (&operation->source);
  g_clear_pointer (&operation->packet, json_node_unref);
  g_free (operation);
}

static void
g_output_stream_splice_cb (GOutputStream *stream,
                           GAsyncResult  *result,
                           gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  TransferOperation *operation = g_task_get_task_data (task);
  gssize transferred, payload_size = 0;
  GError *error = NULL;

  transferred = g_output_stream_splice_finish (stream, result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  payload_size = valent_packet_get_payload_size (operation->packet);
  if (transferred != payload_size)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Expected payload of %zi, received %zi",
                               payload_size, transferred);
      return;
    }

  g_task_return_boolean (task, TRUE);
}

static void
valent_channel_download_cb (ValentChannel *channel,
                            GAsyncResult  *result,
                            gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  TransferOperation *operation = g_task_get_task_data (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GOutputStream) target = NULL;
  GError *error = NULL;

  operation->stream = valent_channel_download_finish (channel, result, &error);
  if (operation->stream == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  target = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (target,
                                g_io_stream_get_input_stream (operation->stream),
                                (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                 G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                (GAsyncReadyCallback)g_output_stream_splice_cb,
                                g_object_ref (task));
}

/**
 * valent_test_download:
 * @channel: a `ValentChannel`
 * @packet: a `JsonNode`
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Simulate downloading the payload described by @packet using @channel.
 *
 * Call [type@Valent.TestFixture.download_finish] to get the result.
 */
void
valent_test_download (ValentChannel       *channel,
                      JsonNode            *packet,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  TransferOperation *operation;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  operation = g_new0 (TransferOperation, 1);
  operation->packet = json_node_ref (packet);

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_test_download);
  g_task_set_task_data (task, operation, transfer_operation_free);
  valent_channel_download (channel,
                           packet,
                           cancellable,
                           (GAsyncReadyCallback)valent_channel_download_cb,
                           g_object_ref (task));
}

/**
 * valent_test_download_finish:
 * @channel: a `ValentChannel`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [type@Valent.TestFixture.download].
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_test_download_finish (ValentChannel  *channel,
                             GAsyncResult   *result,
                             GError        **error)
{
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (g_task_is_valid (result, channel));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
valent_channel_upload_cb (ValentChannel *channel,
                          GAsyncResult  *result,
                          gpointer       user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  TransferOperation *operation = g_task_get_task_data (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  GError *error = NULL;

  operation->stream = valent_channel_upload_finish (channel, result, &error);
  if (operation->stream == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  g_output_stream_splice_async (g_io_stream_get_output_stream (operation->stream),
                                operation->source,
                                (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                 G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                G_PRIORITY_DEFAULT,
                                cancellable,
                                (GAsyncReadyCallback)g_output_stream_splice_cb,
                                g_object_ref (task));
}

static void
g_file_load_bytes_cb (GFile        *file,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentChannel *channel = g_task_get_source_object (task);
  TransferOperation *operation = g_task_get_task_data (task);
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (GBytes) bytes = NULL;
  GError *error = NULL;

  bytes = g_file_load_bytes_finish (file, result, NULL, &error);
  if (bytes == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  operation->source = g_memory_input_stream_new_from_bytes (bytes);
  valent_packet_set_payload_size (operation->packet, g_bytes_get_size (bytes));
  valent_channel_upload (channel,
                         operation->packet,
                         cancellable,
                         (GAsyncReadyCallback)valent_channel_upload_cb,
                         g_object_ref (task));
}

/**
 * valent_test_upload:
 * @channel: a `ValentChannel`
 * @packet: a `JsonNode`
 * @file: a `GFile`
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Simulate uploading @file to the endpoint of @channel.
 *
 * Call [type@Valent.TestFixture.upload_finish] to get the result.
 */
void
valent_test_upload (ValentChannel       *channel,
                    JsonNode            *packet,
                    GFile               *file,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  TransferOperation *operation;

  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (G_IS_FILE (file));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  operation = g_new0 (TransferOperation, 1);
  operation->packet = json_node_ref (packet);

  task = g_task_new (channel, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_test_upload);
  g_task_set_task_data (task, operation, transfer_operation_free);
  g_file_load_bytes_async (file,
                           cancellable,
                           (GAsyncReadyCallback)g_file_load_bytes_cb,
                           g_object_ref (task));
}

/**
 * valent_test_upload_finish:
 * @channel: a `ValentChannel`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [type@Valent.TestFixture.upload].
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_test_upload_finish (ValentChannel  *channel,
                           GAsyncResult   *result,
                           GError        **error)
{
  g_assert (VALENT_IS_CHANNEL (channel));
  g_assert (g_task_is_valid (result, channel));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}

