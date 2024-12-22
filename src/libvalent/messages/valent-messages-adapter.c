// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-messages-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "valent-message.h"
#include "valent-message-attachment.h"
#include "valent-message-thread.h"

#include "valent-messages.h"
#include "valent-messages-adapter.h"
#include "valent-messages-adapter-private.h"

#define GET_THREAD_RQ      "/ca/andyholmes/Valent/sparql/get-thread.rq"
#define GET_THREADS_RQ     "/ca/andyholmes/Valent/sparql/get-threads.rq"
#define SEARCH_MESSAGES_RQ "/ca/andyholmes/Valent/sparql/search-messages.rq"


/**
 * ValentMessagesAdapter:
 *
 * An abstract base class for address book providers.
 *
 * `ValentMessagesAdapter` is a base class for plugins that provide an
 * interface to manage messaging (i.e. SMS/MMS). This usually means loading
 * message history into the SPARQL database and (optionally) sending outgoing
 * messages.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-MessagesAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  TrackerSparqlConnection *connection;
  TrackerNotifier         *notifier;
  TrackerSparqlStatement  *get_thread_stmt;
  TrackerSparqlStatement  *get_threads_stmt;
  GRegex                  *iri_pattern;
  GCancellable            *cancellable;

  /* list model */
  GPtrArray               *items;
} ValentMessagesAdapterPrivate;

static void   g_list_model_iface_init (GListModelInterface *iface);

static void   valent_messages_adapter_load_thread (ValentMessagesAdapter *self,
                                                   const char            *iri);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentMessagesAdapter, valent_messages_adapter, VALENT_TYPE_EXTENSION,
                                  G_ADD_PRIVATE (ValentMessagesAdapter)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

typedef enum
{
  PROP_CONNECTION = 1,
} ValentMessagesAdapterProperty;

static GParamSpec *properties[PROP_CONNECTION + 1] = { NULL, };

static void
valent_messages_adapter_load_thread_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GListModel) list = NULL;
  unsigned int position;
  g_autoptr (GError) error = NULL;

  list = g_task_propagate_pointer (G_TASK (result), &error);
  if (list == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          const char *urn = g_task_get_task_data (G_TASK (result));
          g_warning ("%s(): %s: %s", G_STRFUNC, urn, error->message);
        }

      return;
    }

  position = priv->items->len;
  g_ptr_array_add (priv->items, g_steal_pointer (&list));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static inline gboolean
valent_messages_adapter_equal_func (gconstpointer a,
                                    gconstpointer b)
{
  const char *iri = valent_resource_get_iri ((ValentResource *)a);

  return g_utf8_collate (iri, (const char *)b) == 0;
}

static void
valent_messages_adapter_remove_thread (ValentMessagesAdapter *self,
                                       const char            *iri)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GListModel) item = NULL;
  unsigned int position = 0;

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));

  if (!g_ptr_array_find_with_equal_func (priv->items,
                                         iri,
                                         valent_messages_adapter_equal_func,
                                         &position))
    {
      g_warning ("Resource \"%s\" not found in \"%s\"",
                 iri,
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  item = g_ptr_array_steal_index (priv->items, position);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

static inline gboolean
valent_messages_adapter_event_is_thread (ValentMessagesAdapter *self,
                                         const char            *iri)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  return g_regex_match (priv->iri_pattern, iri, G_REGEX_MATCH_DEFAULT, NULL);
}

static void
on_notifier_event (TrackerNotifier       *notifier,
                   const char            *service,
                   const char            *graph,
                   GPtrArray             *events,
                   ValentMessagesAdapter *self)
{
  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));

  if (g_strcmp0 (VALENT_MESSAGES_GRAPH, graph) != 0)
    return;

  for (unsigned int i = 0; i < events->len; i++)
    {
      TrackerNotifierEvent *event = g_ptr_array_index (events, i);
      const char *urn = tracker_notifier_event_get_urn (event);

      if (!valent_messages_adapter_event_is_thread (self, urn))
        continue;

      switch (tracker_notifier_event_get_event_type (event))
        {
        case TRACKER_NOTIFIER_EVENT_CREATE:
          VALENT_NOTE ("CREATE: %s", urn);
          valent_messages_adapter_load_thread (self, urn);
          break;

        case TRACKER_NOTIFIER_EVENT_DELETE:
          VALENT_NOTE ("DELETE: %s", urn);
          valent_messages_adapter_remove_thread (self, urn);
          break;

        case TRACKER_NOTIFIER_EVENT_UPDATE:
          VALENT_NOTE ("UPDATE: %s", urn);
          // valent_message_adapter_update_thread (self, urn);
          break;

        default:
          g_warn_if_reached ();
        }
    }
}

static gboolean
valent_messages_adapter_open (ValentMessagesAdapter  *self,
                              GError                **error)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  ValentContext *context = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) ontology = NULL;
  const char *iri = NULL;
  g_autofree char *iri_pattern = NULL;

  context = valent_extension_get_context (VALENT_EXTENSION (self));
  file = valent_context_get_cache_file (context, "metadata");
  ontology = g_file_new_for_uri ("resource:///ca/andyholmes/Valent/ontologies/");

  priv->connection =
    tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                   file,
                                   ontology,
                                   NULL,
                                   error);

  if (priv->connection == NULL)
    return FALSE;

  iri = valent_resource_get_iri (VALENT_RESOURCE (self));
  iri_pattern = g_strdup_printf ("^%s:([^:]+)$", iri);
  priv->iri_pattern = g_regex_new (iri_pattern,
                                   G_REGEX_OPTIMIZE,
                                   G_REGEX_MATCH_DEFAULT,
                                   NULL);

  priv->notifier = tracker_sparql_connection_create_notifier (priv->connection);
  g_signal_connect_object (priv->notifier,
                           "events",
                           G_CALLBACK (on_notifier_event),
                           self,
                           G_CONNECT_DEFAULT);

  return TRUE;
}

/*
 * GListModel
 */
static gpointer
valent_messages_adapter_get_item (GListModel   *list,
                                  unsigned int  position)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (list);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autofree char *iri = NULL;
  g_autoptr (ValentMessage) latest_message = NULL;
  g_auto (GStrv) participants = NULL;

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));

  if G_UNLIKELY (position >= priv->items->len)
    return NULL;

  // FIXME: a duplicate thread is returned to avoid accruing memory
  // return g_object_ref (g_ptr_array_index (priv->items, position));
  g_object_get (g_ptr_array_index (priv->items, position),
                "iri",            &iri,
                "latest-message", &latest_message,
                "participants",   &participants,
                NULL);

  return g_object_new (VALENT_TYPE_MESSAGE_THREAD,
                       "connection",     priv->connection,
                       "notifier",       priv->notifier,
                       "iri",            iri,
                       "latest-message", latest_message,
                       "participants",   participants,
                       NULL);
}

static GType
valent_messages_adapter_get_item_type (GListModel *list)
{
  return G_TYPE_LIST_MODEL;
}

static unsigned int
valent_messages_adapter_get_n_items (GListModel *list)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (list);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));

  return priv->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_messages_adapter_get_item;
  iface->get_item_type = valent_messages_adapter_get_item_type;
  iface->get_n_items = valent_messages_adapter_get_n_items;
}

/*
 * ValentMessagesAdapterPrivate
 *
 */
ValentMessage *
valent_message_from_sparql_cursor (TrackerSparqlCursor *cursor,
                                   ValentMessage       *current)
{
  ValentMessage *ret = NULL;
  int64_t message_id;

  g_assert (TRACKER_IS_SPARQL_CURSOR (cursor));
  g_assert (current == NULL || VALENT_IS_MESSAGE (current));

  message_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_ID);
  if (current != NULL && valent_message_get_id (current) == message_id)
    {
      ret = g_object_ref (current);
    }
  else
    {
      const char *iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_IRI, NULL);
      g_autoptr (GListStore) attachments = NULL;
      ValentMessageBox box = VALENT_MESSAGE_BOX_ALL;
      int64_t date = 0;
      g_autoptr (GDateTime) datetime = NULL;
      gboolean read = FALSE;
      const char *recipients = NULL;
      g_auto (GStrv) recipientv = NULL;
      const char *sender = NULL;
      int64_t subscription_id = -1;
      const char *text = NULL;
      int64_t thread_id = -1;

      attachments = g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT);
      box = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_BOX);

      datetime = tracker_sparql_cursor_get_datetime (cursor, CURSOR_MESSAGE_DATE);
      if (datetime != NULL)
        date = g_date_time_to_unix_usec (datetime) / 1000;

      read = tracker_sparql_cursor_get_boolean (cursor, CURSOR_MESSAGE_READ);

      recipients = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_RECIPIENTS, NULL);
      if (recipients != NULL)
        recipientv = g_strsplit (recipients, ",", -1);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_SENDER))
        sender = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_SENDER, NULL);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_SUBSCRIPTION_ID))
        subscription_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_SUBSCRIPTION_ID);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_TEXT))
        text = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_TEXT, NULL);

      thread_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_THREAD_ID);

      ret = g_object_new (VALENT_TYPE_MESSAGE,
                          "iri",             iri,
                          "box",             box,
                          "date",            date,
                          "id",              message_id,
                          "read",            read,
                          "recipients",      recipientv,
                          "sender",          sender,
                          "subscription-id", subscription_id,
                          "text",            text,
                          "thread-id",       thread_id,
                          "attachments",     attachments,
                          NULL);
    }

  /* Attachment
   */
  if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_ATTACHMENT_IRI))
    {
      const char *iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_ATTACHMENT_IRI, NULL);
      GListModel *attachments = valent_message_get_attachments (ret);
      g_autoptr (ValentMessageAttachment) attachment = NULL;
      g_autoptr (GIcon) preview = NULL;
      g_autoptr (GFile) file = NULL;

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_ATTACHMENT_PREVIEW))
        {
          const char *base64_data;

          base64_data = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_ATTACHMENT_PREVIEW, NULL);
          if (base64_data != NULL)
            {
              g_autoptr (GBytes) bytes = NULL;
              unsigned char *data;
              size_t len;

              data = g_base64_decode (base64_data, &len);
              bytes = g_bytes_new_take (g_steal_pointer (&data), len);
              preview = g_bytes_icon_new (bytes);
            }
        }

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_ATTACHMENT_FILE))
        {
          const char *file_uri;

          file_uri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_ATTACHMENT_FILE, NULL);
          if (file_uri != NULL)
            file = g_file_new_for_uri (file_uri);
        }

      attachment = g_object_new (VALENT_TYPE_MESSAGE_ATTACHMENT,
                                 "iri",     iri,
                                 "preview", preview,
                                 "file",    file,
                                 NULL);
      g_list_store_append (G_LIST_STORE (attachments), attachment);
    }

  return g_steal_pointer (&ret);
}

static ValentMessageThread *
valent_message_thread_from_sparql_cursor (ValentMessagesAdapter *self,
                                          TrackerSparqlCursor   *cursor)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (ValentMessage) message = NULL;
  const char *iri = NULL;
  const char *participants = NULL;
  g_auto (GStrv) participantv = NULL;

  g_assert (TRACKER_IS_SPARQL_CURSOR (cursor));

  /* NOTE: typically there won't be a thread without a message, but this may be
   *       the case as an implementation detail.
   */
  iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_IRI, NULL);
  if (iri != NULL)
    {
      g_autoptr (GListStore) attachments = NULL;
      ValentMessageBox box = VALENT_MESSAGE_BOX_ALL;
      int64_t date = 0;
      g_autoptr (GDateTime) datetime = NULL;
      int64_t message_id;
      gboolean read = FALSE;
      const char *recipients = NULL;
      g_auto (GStrv) recipientv = NULL;
      const char *sender = NULL;
      int64_t subscription_id = -1;
      const char *text = NULL;
      int64_t thread_id = -1;

      attachments = g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT);
      box = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_BOX);

      datetime = tracker_sparql_cursor_get_datetime (cursor, CURSOR_MESSAGE_DATE);
      if (datetime != NULL)
        date = g_date_time_to_unix_usec (datetime) / 1000;

      message_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_ID);
      read = tracker_sparql_cursor_get_boolean (cursor, CURSOR_MESSAGE_READ);

      recipients = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_RECIPIENTS, NULL);
      if (recipients != NULL)
        recipientv = g_strsplit (recipients, ",", -1);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_SENDER))
        sender = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_SENDER, NULL);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_SUBSCRIPTION_ID))
        subscription_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_SUBSCRIPTION_ID);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_TEXT))
        text = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_TEXT, NULL);

      thread_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_THREAD_ID);

      message = g_object_new (VALENT_TYPE_MESSAGE,
                              "iri",             iri,
                              "box",             box,
                              "date",            date,
                              "id",              message_id,
                              "read",            read,
                              "recipients",      recipientv,
                              "sender",          sender,
                              "subscription-id", subscription_id,
                              "text",            text,
                              "thread-id",       thread_id,
                              "attachments",     attachments,
                              NULL);
    }

  /* Thread
   */
  if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_THREAD_IRI))
    iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_THREAD_IRI, NULL);

  participants = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_THREAD_PARTICIPANTS, NULL);
  if (participants != NULL)
    participantv = g_strsplit (participants, ",", -1);

  return g_object_new (VALENT_TYPE_MESSAGE_THREAD,
                       "connection",     tracker_sparql_cursor_get_connection (cursor),
                       "notifier",       priv->notifier,
                       "iri",            iri,
                       "latest-message", message,
                       "participants",   participantv,
                       NULL);
}

static void
cursor_get_threads_cb (TrackerSparqlCursor *cursor,
                       GAsyncResult        *result,
                       gpointer             user_data)
{
  g_autoptr (ValentMessagesAdapter) self = VALENT_MESSAGES_ADAPTER (g_steal_pointer (&user_data));
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      ValentMessageThread *thread = NULL;

      thread = valent_message_thread_from_sparql_cursor (self, cursor);
      if (thread != NULL)
        {
          unsigned int position;

          position = priv->items->len;
          g_ptr_array_add (priv->items, g_steal_pointer (&thread));
          g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
        }

      tracker_sparql_cursor_next_async (cursor,
                                        priv->cancellable,
                                        (GAsyncReadyCallback) cursor_get_threads_cb,
                                        g_object_ref (self));
    }
  else
    {
      if (error != NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      tracker_sparql_cursor_close (cursor);
    }
}

static void
execute_get_threads_cb (TrackerSparqlStatement *stmt,
                        GAsyncResult           *result,
                        gpointer                user_data)
{
  g_autoptr (ValentMessagesAdapter) self = VALENT_MESSAGES_ADAPTER (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  g_autoptr (GError) error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    g_task_get_cancellable (G_TASK (result)),
                                    (GAsyncReadyCallback) cursor_get_threads_cb,
                                    g_object_ref (self));
}

static void
valent_messages_adapter_load_threads (ValentMessagesAdapter *self)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));
  g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (priv->connection));

  if (priv->cancellable != NULL)
    return;

  priv->cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  if (priv->get_threads_stmt == NULL)
    {
      priv->get_threads_stmt =
        tracker_sparql_connection_load_statement_from_gresource (priv->connection,
                                                                 GET_THREADS_RQ,
                                                                 priv->cancellable,
                                                                 &error);
    }

  if (priv->get_threads_stmt == NULL)
    {
      if (error != NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  tracker_sparql_statement_execute_async (priv->get_threads_stmt,
                                          priv->cancellable,
                                          (GAsyncReadyCallback) execute_get_threads_cb,
                                          g_object_ref (self));
}

static void
cursor_get_thread_cb (TrackerSparqlCursor *cursor,
                      GAsyncResult        *result,
                      gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentMessagesAdapter *self = g_task_get_source_object (task);
  g_autoptr (ValentMessageThread) thread = NULL;
  GError *error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    thread = valent_message_thread_from_sparql_cursor (self, cursor);

  if (thread != NULL)
    {
      g_task_return_pointer (task, g_object_ref (thread), g_object_unref);
    }
  else
    {
      if (error == NULL)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to find thread");
        }

      g_task_return_error (task, g_steal_pointer (&error));
    }

  tracker_sparql_cursor_close (cursor);
}

static void
execute_get_thread_cb (TrackerSparqlStatement *stmt,
                       GAsyncResult           *result,
                       gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GError *error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_get_thread_cb,
                                    g_object_ref (task));
}

static void
valent_messages_adapter_load_thread (ValentMessagesAdapter *self,
                                     const char            *iri)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));
  g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (priv->connection));

  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  task = g_task_new (self, cancellable, valent_messages_adapter_load_thread_cb, NULL);
  g_task_set_source_tag (task, valent_messages_adapter_load_thread);

  if (priv->get_thread_stmt == NULL)
    {
      priv->get_thread_stmt =
        tracker_sparql_connection_load_statement_from_gresource (priv->connection,
                                                                 GET_THREAD_RQ,
                                                                 cancellable,
                                                                 &error);
    }

  if (priv->get_thread_stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_statement_bind_string (priv->get_thread_stmt, "iri", iri);
  tracker_sparql_statement_execute_async (priv->get_thread_stmt,
                                          cancellable,
                                          (GAsyncReadyCallback) execute_get_thread_cb,
                                          g_object_ref (task));
}

/*
 * ValentMessagesAdapter
 */
/* LCOV_EXCL_START */
static void
valent_messages_adapter_real_send_message (ValentMessagesAdapter *adapter,
                                           ValentMessage         *message,
                                           GCancellable          *cancellable,
                                           GAsyncReadyCallback    callback,
                                           gpointer               user_data)
{
  g_assert (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_assert (VALENT_IS_MESSAGE (message));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (adapter, callback, user_data,
                           valent_messages_adapter_real_send_message,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement send_message",
                           G_OBJECT_TYPE_NAME (adapter));
}

static gboolean
valent_messages_adapter_real_send_message_finish (ValentMessagesAdapter  *adapter,
                                                  GAsyncResult           *result,
                                                  GError                **error)
{
  g_assert (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}
/* LCOV_EXCL_STOP */

/*
 * ValentObject
 */
static void
valent_messages_adapter_destroy (ValentObject *object)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  g_clear_object (&priv->get_thread_stmt);
  g_clear_object (&priv->get_threads_stmt);
  g_clear_pointer (&priv->iri_pattern, g_regex_unref);

  if (priv->notifier != NULL)
    {
      g_signal_handlers_disconnect_by_func (priv->notifier, on_notifier_event, self);
      g_clear_object (&priv->notifier);
    }

  if (priv->connection != NULL)
    {
      tracker_sparql_connection_close (priv->connection);
      g_clear_object (&priv->connection);
    }

  VALENT_OBJECT_CLASS (valent_messages_adapter_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_messages_adapter_constructed (GObject *object)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  G_OBJECT_CLASS (valent_messages_adapter_parent_class)->constructed (object);

  if (priv->connection == NULL)
    {
      if (!valent_messages_adapter_open (self, &error))
        g_critical ("%s(): %s", G_STRFUNC, error->message);
    }

  valent_messages_adapter_load_threads (self);
}

static void
valent_messages_adapter_finalize (GObject *object)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  g_clear_object (&priv->cancellable);
  g_clear_pointer (&priv->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_messages_adapter_parent_class)->finalize (object);
}

static void
valent_messages_adapter_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  switch ((ValentMessagesAdapterProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_messages_adapter_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  switch ((ValentMessagesAdapterProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_assert (priv->connection == NULL);
      priv->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_messages_adapter_class_init (ValentMessagesAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_messages_adapter_constructed;
  object_class->finalize = valent_messages_adapter_finalize;
  object_class->get_property = valent_messages_adapter_get_property;
  object_class->set_property = valent_messages_adapter_set_property;

  vobject_class->destroy = valent_messages_adapter_destroy;

  klass->send_message = valent_messages_adapter_real_send_message;
  klass->send_message_finish = valent_messages_adapter_real_send_message_finish;

  /**
   * ValentMessagesAdapter:connection:
   *
   * The database connection.
   */
  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                          TRACKER_TYPE_SPARQL_CONNECTION,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_messages_adapter_init (ValentMessagesAdapter *self)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_messages_adapter_send_message: (virtual send_message)
 * @adapter: a `ValentMessagesAdapter`
 * @message: the message to send
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Send @message via @adapter.
 *
 * Call [method@Valent.MessagesAdapter.send_message_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_send_message (ValentMessagesAdapter *adapter,
                                      ValentMessage         *message,
                                      GCancellable          *cancellable,
                                      GAsyncReadyCallback    callback,
                                      gpointer               user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MESSAGE (message));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_MESSAGES_ADAPTER_GET_CLASS (adapter)->send_message (adapter,
                                                             message,
                                                             cancellable,
                                                             callback,
                                                             user_data);

  VALENT_EXIT;
}

/**
 * valent_messages_adapter_send_message_finish: (virtual send_message_finish)
 * @adapter: a `ValentMessagesAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [method@Valent.MessagesAdapter.send_message].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_messages_adapter_send_message_finish (ValentMessagesAdapter  *adapter,
                                             GAsyncResult            *result,
                                             GError                 **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, adapter), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_MESSAGES_ADAPTER_GET_CLASS (adapter)->send_message_finish (adapter,
                                                                          result,
                                                                          error);

  VALENT_RETURN (ret);
}

