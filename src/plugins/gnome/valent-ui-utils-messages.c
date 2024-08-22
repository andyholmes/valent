// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-messages"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>

#include "valent-ui-utils-private.h"

/*< private>
 *
 * Cursor columns for `vmo:PhoneMessage`.
 */
#define CURSOR_MESSAGE_IRI                0
#define CURSOR_MESSAGE_BOX                1
#define CURSOR_MESSAGE_DATE               2
#define CURSOR_MESSAGE_ID                 3
#define CURSOR_MESSAGE_READ               4
#define CURSOR_MESSAGE_RECIPIENTS         5
#define CURSOR_MESSAGE_SENDER             6
#define CURSOR_MESSAGE_SUBSCRIPTION_ID    7
#define CURSOR_MESSAGE_TEXT               8
#define CURSOR_MESSAGE_THREAD_ID          9
#define CURSOR_MESSAGE_ATTACHMENT_IRI     10
#define CURSOR_MESSAGE_ATTACHMENT_PREVIEW 11
#define CURSOR_MESSAGE_ATTACHMENT_FILE    12

#define SEARCH_MESSAGES_RQ "/ca/andyholmes/Valent/sparql/search-messages.rq"


static void
cursor_lookup_thread_cb (TrackerSparqlCursor *cursor,
                         GAsyncResult        *result,
                         gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error) &&
      tracker_sparql_cursor_is_bound (cursor, 0))
    {
      const char *iri = NULL;

      iri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
      g_task_return_pointer (task, g_strdup (iri), g_free);
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
execute_lookup_thread_cb (TrackerSparqlStatement *stmt,
                          GAsyncResult           *result,
                          gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GCancellable *cancellable = NULL;
  GError *error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  cancellable = g_task_get_cancellable (G_TASK (result));
  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_lookup_thread_cb,
                                    g_object_ref (task));
}

#define LOOKUP_THREAD_FMT                                        \
"SELECT DISTINCT ?communicationChannel                           \
WHERE {                                                          \
  VALUES ?specifiedIRIs { %s }                                   \
  ?communicationChannel vmo:hasParticipant ?participant .        \
  FILTER (?participant IN (%s))                                  \
  FILTER NOT EXISTS {                                            \
    ?communicationChannel vmo:hasParticipant ?otherParticipant . \
    FILTER (?otherParticipant NOT IN (%s))                       \
  }                                                              \
}                                                                \
GROUP BY ?communicationChannel                                   \
HAVING (COUNT(DISTINCT ?participant) = %u)"

/**
 * valent_messages_adapter_lookup_thread:
 * @adapter: a `ValentMessagesAdapter`
 * @participants: a list of contact mediums
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Find the thread with @participants.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_lookup_thread (ValentMessagesAdapter *adapter,
                                       const char * const    *participants,
                                       GCancellable          *cancellable,
                                       GAsyncReadyCallback    callback,
                                       gpointer               user_data)
{
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (TrackerSparqlStatement) stmt = NULL;
  g_autoptr (GTask) task = NULL;
  g_autoptr (GStrvBuilder) builder = NULL;
  g_auto (GStrv) iriv = NULL;
  g_autofree char *iris = NULL;
  g_autofree char *values = NULL;
  g_autofree char *sparql = NULL;
  GError *error = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (participants != NULL && participants[0] != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_messages_adapter_lookup_thread);
  g_task_set_task_data (task,
                        g_ptr_array_new_with_free_func (g_object_unref),
                        (GDestroyNotify)g_ptr_array_unref);

  builder = g_strv_builder_new ();
  for (size_t i = 0; participants[i] != NULL; i++)
    {
      g_autofree char *iri = NULL;

      if (g_strrstr (participants[i], "@"))
        {
          iri = g_strdup_printf ("<mailto:%s>", participants[i]);
        }
      else
        {
          g_autoptr (EPhoneNumber) number = NULL;

          number = e_phone_number_from_string (participants[i], NULL, NULL);
          if (number != NULL)
            {
              g_autofree char *uri = NULL;

              uri = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_RFC3966);
              iri = g_strdup_printf ("<%s>", uri);
            }
        }

      if (iri != NULL)
        g_strv_builder_take (builder, g_steal_pointer (&iri));
    }
  iriv = g_strv_builder_end (builder);

  iris = g_strjoinv (", ", iriv);
  values = g_strjoinv (" ", iriv);
  sparql = g_strdup_printf (LOOKUP_THREAD_FMT,
                            values,
                            iris,
                            iris,
                            g_strv_length ((GStrv)iriv));

  g_object_get (adapter, "connection", &connection, NULL);
  stmt = tracker_sparql_connection_query_statement (connection,
                                                    sparql,
                                                    cancellable,
                                                    &error);

  if (stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      VALENT_EXIT;
    }

  tracker_sparql_statement_execute_async (stmt,
                                          cancellable,
                                          (GAsyncReadyCallback) execute_lookup_thread_cb,
                                          g_object_ref (task));

  VALENT_EXIT;
}

/**
 * valent_messages_adapter_lookup_thread_finish:
 * @adapter: a `ValentMessagesAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by valent_contact_adapter_lookup_contact().
 *
 * Returns: (transfer full): an `EContact`
 */
GListModel *
valent_messages_adapter_lookup_thread_finish (ValentMessagesAdapter  *adapter,
                                              GAsyncResult           *result,
                                              GError                **error)
{
  GListModel *ret = NULL;
  g_autofree char *iri = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  iri = g_task_propagate_pointer (G_TASK (result), error);
  if (iri != NULL)
    {
      unsigned int n_threads = g_list_model_get_n_items (G_LIST_MODEL (adapter));

      for (unsigned int i = 0; i < n_threads; i++)
        {
          g_autoptr (GListModel) thread = NULL;
          g_autofree char *thread_iri = NULL;

          thread = g_list_model_get_item (G_LIST_MODEL (adapter), i);
          g_object_get (thread, "iri", &thread_iri, NULL);

          if (g_strcmp0 (iri, thread_iri) == 0)
            {
              ret = g_steal_pointer (&thread);
              break;
            }
        }
    }

  VALENT_RETURN (ret);
}

static ValentMessage *
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

static void
cursor_search_cb (TrackerSparqlCursor *cursor,
                  GAsyncResult        *result,
                  gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GListStore *messages = g_task_get_task_data (task);
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      g_autoptr (ValentMessage) current = NULL;
      g_autoptr (ValentMessage) message = NULL;
      unsigned int n_items = 0;

      n_items = g_list_model_get_n_items (G_LIST_MODEL (messages));
      if (n_items > 0)
        current = g_list_model_get_item (G_LIST_MODEL (messages), n_items - 1);

      message = valent_message_from_sparql_cursor (cursor, current);
      if (message != current)
        g_list_store_append (messages, message);

      tracker_sparql_cursor_next_async (cursor,
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback) cursor_search_cb,
                                        g_object_ref (task));
      return;
    }

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_object_ref (messages), g_object_unref);

  tracker_sparql_cursor_close (cursor);
}

static void
execute_search_cb (TrackerSparqlStatement *stmt,
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
                                    (GAsyncReadyCallback) cursor_search_cb,
                                    g_object_ref (task));
}

/**
 * valent_messages_adapter_search:
 * @adapter: a `ValentMessagesAdapter`
 * @query: a string to search for
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Search through all the messages in @adapter and return the most recent message
 * from each thread containing @query.
 *
 * Call [method@Valent.MessagesAdapter.search_messages_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_search (ValentMessagesAdapter  *adapter,
                             const char          *query,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr (TrackerSparqlStatement) stmt = NULL;
  g_autoptr (GTask) task = NULL;
  g_autofree char *query_sanitized = NULL;
  GError *error = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (query != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_messages_adapter_search);
  g_task_set_task_data (task, g_list_store_new (VALENT_TYPE_MESSAGE), g_object_unref);

  stmt = g_object_dup_data (G_OBJECT (adapter),
                            "valent-message-adapter-search",
                            (GDuplicateFunc)((GCallback)g_object_ref),
                            NULL);

  if (stmt == NULL)
    {
      g_autoptr (TrackerSparqlConnection) connection = NULL;

      g_object_get (adapter, "connection", &connection, NULL);
      stmt = tracker_sparql_connection_load_statement_from_gresource (connection,
                                                                      SEARCH_MESSAGES_RQ,
                                                                      cancellable,
                                                                      &error);

      if (stmt == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      g_object_set_data_full (G_OBJECT (adapter),
                              "valent-message-adapter-search",
                              g_object_ref (stmt),
                              g_object_unref);
    }

  query_sanitized = tracker_sparql_escape_string (query);
  tracker_sparql_statement_bind_string (stmt, "query", query_sanitized);
  tracker_sparql_statement_execute_async (stmt,
                                          g_task_get_cancellable (task),
                                          (GAsyncReadyCallback) execute_search_cb,
                                          g_object_ref (task));

  VALENT_EXIT;
}

/**
 * valent_messages_adapter_search_finish:
 * @adapter: a `ValentMessagesAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [method@Valent.MessagesAdapter.search_messages].
 *
 * Returns: (transfer full) (element-type Valent.Message): a list of messages
 *
 * Since: 1.0
 */
GListModel *
valent_messages_adapter_search_finish (ValentMessagesAdapter  *adapter,
                                    GAsyncResult        *result,
                                    GError             **error)
{
  GListModel *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

