// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-store"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-sms-store.h"
#include "valent-sms-store-private.h"

#include "valent-sql-db.h"
#include "valent-sql-stmt.h"


struct _ValentSmsStore
{
  ValentData   parent_instance;

  ValentSqlDb *db;
  GListStore  *summary;
};

G_DEFINE_TYPE (ValentSmsStore, valent_sms_store, VALENT_TYPE_DATA)

enum {
  MESSAGE_ADDED,
  MESSAGE_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static ValentSmsMessage *
deserialize_message (ValentSqlStmt *stmt)
{
  g_assert (stmt != NULL);

  return g_object_new (VALENT_TYPE_SMS_MESSAGE,
                       "box",       valent_sql_stmt_get_int (stmt, 0),
                       "date",      valent_sql_stmt_get_int64 (stmt, 1),
                       "id",        valent_sql_stmt_get_int64 (stmt, 2),
                       "metadata",  valent_sql_stmt_get_variant (stmt, 3),
                       "read",      valent_sql_stmt_get_int (stmt, 4),
                       "sender",    valent_sql_stmt_get_string (stmt, 5),
                       "text",      valent_sql_stmt_get_string (stmt, 6),
                       "thread_id", valent_sql_stmt_get_int64 (stmt, 7),
                       NULL);
}

static gboolean
check_db (ValentSmsStore  *self,
          GError         **error)
{
  const char *cache_path;
  g_autofree char *db_path = NULL;

  if (self->db != NULL)
    return TRUE;

  cache_path = valent_data_get_cache_path (VALENT_DATA (self));
  db_path = g_build_filename (cache_path, "sms.db", NULL);
  self->db = valent_sql_db_new (db_path);

  valent_sql_db_lock (self->db);

  if (!valent_sql_db_open (self->db, error) ||
      !valent_sql_db_exec (self->db, MESSAGE_TABLE_SQL, error) ||
      !valent_sql_db_exec (self->db, PARTICIPANT_TABLE_SQL, error))
    return FALSE;

  valent_sql_db_unlock (self->db);

  return TRUE;
}

/*
 * GObject
 */
static void
valent_sms_store_finalize (GObject *object)
{
  ValentSmsStore *self = VALENT_SMS_STORE (object);

  g_clear_weak_pointer (&self->summary);

  G_OBJECT_CLASS (valent_sms_store_parent_class)->finalize (object);
}

static void
valent_sms_store_class_init (ValentSmsStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_sms_store_finalize;

  /**
   * ValentSmsStore::message-added:
   * @store: a #ValentSmsStore
   * @thread_id: the thread ID @message was added to
   * @message: a #ValentMessage
   *
   * ValentSmsStore::message-added is emitted when a new message is added to
   * @store.
   */
  signals [MESSAGE_ADDED] =
    g_signal_new ("message-added",
                  VALENT_TYPE_SMS_STORE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_INT64, VALENT_TYPE_SMS_MESSAGE);

  /**
   * ValentSmsStore::message-removed:
   * @store: a #ValentSmsStore
   * @thread_id: the thread ID @message was removed from
   * @message_id: the message ID
   * @message: a #ValentMessage
   *
   * ValentSmsStore::message-removed is emitted when a message is removed from
   * @store. If @message_id is FIXME then @thread_id is being removed.
   */
  signals [MESSAGE_REMOVED] =
    g_signal_new ("message-removed",
                  VALENT_TYPE_SMS_STORE,
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, G_TYPE_INT64, G_TYPE_INT64);
}

static void
valent_sms_store_init (ValentSmsStore *self)
{
}

/**
 * valent_sms_store_new:
 * @parent: a #ValentData
 *
 * Create a new #ValentSmsStore.
 *
 * Returns: (transfer full): a new sms store
 */
ValentSmsStore *
valent_sms_store_new (ValentData *parent)
{
  return g_object_new (VALENT_TYPE_SMS_STORE,
                       "parent",  parent,
                       "context", "sms",
                       NULL);
}

static void
valent_sms_store_add_participant (ValentSmsStore *store,
                                  gint64          thread_id,
                                  const char     *address)
{
  g_autoptr (GError) error = NULL;
  const char *sql;
  g_autoptr (ValentSqlStmt) stmt = NULL;

  g_return_if_fail (check_db (store, NULL));

  sql = "INSERT INTO participant (thread_id, address) VALUES (?, ?);";
  stmt = valent_sql_db_prepare (store->db, sql, &error);

  if (stmt == NULL)
    {
      g_warning ("Adding sms participant: %s", error->message);
      return;
    }

  valent_sql_stmt_set_int64 (stmt, 1, thread_id);
  valent_sql_stmt_set_string (stmt, 2, address);

  if (!valent_sql_db_stmt (store->db, stmt, &error))
    g_warning ("[%s] participant: %s", G_STRFUNC, error->message);
}

/**
 * valent_sms_store_add_message_full:
 * @box: a #ValentMessageBox enum
 * @date: a UNIX epoch timestamp (ms)
 * @id: a unique message ID
 * @read: viewed status
 * @sender: the sender address
 * @text: the message content
 * @thread_id: a grouping ID
 *
 * Adds a message by it's properties. This is convenient for avoiding an
 * intermediate #ValentMessage object.
 */
void
valent_sms_store_add_message_full  (ValentSmsStore      *store,
                                    ValentSmsMessageBox  box,
                                    gint64               date,
                                    gint64               id,
                                    GVariant            *metadata,
                                    gboolean             read,
                                    const char          *sender,
                                    const char          *text,
                                    gint64               thread_id)
{
  g_autoptr (ValentSqlStmt) stmt = NULL;
  g_autoptr (GVariant) addresses = NULL;
  g_autoptr (GError) error = NULL;
  g_autofree char *metadata_str = NULL;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (metadata == NULL || g_variant_is_of_type (metadata, G_VARIANT_TYPE_DICTIONARY));
  g_return_if_fail (check_db (store, NULL));


  /* Addresses Metadata
   *
   * The `addresses` key in the metadata holds a list of address objects
   * representing recipients of the message (excluding us). If the message
   * is incoming, then the first address in the array is the sender. Each object
   * has at minimum the string member `address`.
   *
   * We use this information to populate the secondary database `participants`
   * which makes things easier later in #ValentSmsConversation.
   *
   * [
   *   {address: "555-555-5555"}, // Participant 1 (Sender if incoming)
   *   {address: "123-456-7890"}, // Participant 2
   *   ...
   * ]
   */
  if (g_variant_lookup (metadata, "addresses", "@aa{sv}", &addresses))
    {
      guint i, n_addresses;

      n_addresses = g_variant_n_children (addresses);

      for (i = 0; i < n_addresses; i++)
        {
          g_autoptr (GVariant) participant = NULL;
          const char *address;

          participant = g_variant_get_child_value (addresses, i);

          if (g_variant_lookup (participant, "address", "&s", &address))
            {
              //valent_sms_store_add_participant (store, thread_id, address);
            }
        }
    }
  metadata_str = g_variant_print (metadata, TRUE);

  /* Add to database */
  stmt = valent_sql_db_prepare (store->db, ADD_MESSAGE_SQL, &error);

  if (stmt == NULL)
    {
      g_warning ("Adding sms message: %s", error->message);
      return;
    }

  valent_sql_stmt_set_int (stmt, 1, box);
  valent_sql_stmt_set_int64 (stmt, 2, date);
  valent_sql_stmt_set_int64 (stmt, 3, date); // FIXME: id
  valent_sql_stmt_set_string (stmt, 4, metadata_str);
  valent_sql_stmt_set_int (stmt, 5, read);
  valent_sql_stmt_set_string (stmt, 6, sender);
  valent_sql_stmt_set_string (stmt, 7, text);
  valent_sql_stmt_set_int64 (stmt, 8, thread_id);

  if (!valent_sql_db_stmt (store->db, stmt, &error))
    {
      g_warning ("[%s] Adding sms message: %s", G_STRFUNC, error->message);
      return;
    }
}

/**
 * valent_sms_store_add_message:
 * @store: a #ValentSmsStore
 * @message: a #ValentMessage
 *
 * Add @node, a single message from a `kdeconnect.sms.messages` packet, to
 * @store, including an entry for each participant in the thread.
 */
void
valent_sms_store_add_message (ValentSmsStore   *store,
                              ValentSmsMessage *message)
{
  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (VALENT_IS_SMS_MESSAGE (message));
  g_return_if_fail (check_db (store, NULL));

  valent_sms_store_add_message_full (store,
                                     valent_sms_message_get_box (message),
                                     valent_sms_message_get_date (message),
                                     valent_sms_message_get_id (message),
                                     valent_sms_message_get_metadata (message),
                                     valent_sms_message_get_read (message),
                                     valent_sms_message_get_sender (message),
                                     valent_sms_message_get_text (message),
                                     valent_sms_message_get_thread_id (message));

  g_signal_emit (G_OBJECT (store),
                 signals [MESSAGE_ADDED],
                 0,
                 valent_sms_message_get_thread_id (message),
                 message);
}

/**
 * valent_sms_store_remove_message:
 * @store: a #ValentSmsStore
 * @thread_id: a thread ID
 * @message_id: a message ID
 *
 * Remove the message with @message_id from @thread_id.
 */
void
valent_sms_store_remove_message (ValentSmsStore *store,
                                 gint64          thread_id,
                                 gint64          message_id)
{
  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (thread_id > 0);
  g_return_if_fail (message_id > 0);
  g_return_if_fail (check_db (store, NULL));

  /* TODO: Remove SQL entry */

  /* Signal removal */
  g_signal_emit (G_OBJECT (store),
                 signals [MESSAGE_REMOVED],
                 0,
                 thread_id,
                 message_id);
}

/**
 * valent_sms_store_remove_messages:
 * @store: a #ValentSmsStore
 * @thread_id: a #ValentSms id
 *
 * Remove @thread_id and all it's messages from @store.
 */
void
valent_sms_store_remove_messages (ValentSmsStore *store,
                                  gint64          thread_id)
{
  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (thread_id > 0);
  g_return_if_fail (check_db (store, NULL));

  /* TODO: Remove SQL entries */

  g_signal_emit (G_OBJECT (store),
                 signals [MESSAGE_REMOVED],
                 0,
                 thread_id,
                 -1);
}

/**
 * valent_sms_store_find:
 * @store: a #ValentSmsStore
 * @query: a query
 * @error: (nullable): a #GError
 *
 * ...
 */
GPtrArray *
valent_sms_store_find (ValentSmsStore  *store,
                       const char      *query,
                       GError         **error)
{
  GPtrArray *results = NULL;
  g_autoptr (ValentSqlStmt) stmt = NULL;
  g_autofree char *query_param = NULL;

  g_assert (VALENT_IS_SMS_STORE (store));

  if (!check_db (store, error))
    return NULL;

  /* Lock during the transaction */
  valent_sql_db_lock (store->db);

  results = g_ptr_array_new ();
  stmt = valent_sql_db_prepare (store->db, FIND_SQL, error);

  if (stmt == NULL)
    {
      valent_sql_db_unlock (store->db);
      return results;
    }

  query_param = g_strdup_printf ("%%%s%%", query);
  valent_sql_stmt_set_string (stmt, 1, query_param);

  while (valent_sql_db_step (store->db, stmt, error) == VALENT_SQL_STEP_ROW)
    {
      ValentSmsMessage *message;

      message = deserialize_message (stmt);

      if (message != NULL)
        g_ptr_array_add (results, message);
    }

  valent_sql_db_unlock (store->db);

  return results;
}

static void
message_results_free (gpointer data)
{
  g_ptr_array_set_free_func ((GPtrArray *)data, g_object_unref);
}

static void
find_thread (GTask        *task,
             gpointer      source_object,
             gpointer      task_data,
             GCancellable *cancellable)
{
  GPtrArray *results = NULL;
  GError *error = NULL;
  ValentSmsStore *store = source_object;
  const char *query = task_data;

  if (g_task_return_error_if_cancelled (task))
    return;

  results = valent_sms_store_find (store, query, &error);

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, results, message_results_free);
}

/**
 * valent_sms_store_find_async:
 * @store: a #ValentSmsStore
 * @query: a string to search for
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Asynchronous wrapper around valent_sms_store_find(). Call
 * valent_sms_store_find_finish() to get the result.
 */
void
valent_sms_store_find_async (ValentSmsStore      *store,
                             const char          *query,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (query != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_sms_store_find_async);
  g_task_set_task_data (task, g_strdup (query), g_free);
  g_task_run_in_thread (task, find_thread);
}

/**
 * valent_sms_store_find_finish:
 * @store: a #ValentMessages
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_sms_store_find_async().
 *
 * Returns: (transfer full) (element-type Valent.Message): an #GPtrArray
 */
GPtrArray *
valent_sms_store_find_finish (ValentSmsStore  *store,
                              GAsyncResult    *result,
                              GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, store), FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
}


static inline int
summary_sort (gconstpointer a,
              gconstpointer b,
              gpointer      user_data)
{
  gint64 date1 = valent_sms_message_get_date ((ValentSmsMessage *)a);
  gint64 date2 = valent_sms_message_get_date ((ValentSmsMessage *)b);

  return (date1 < date2) ? -1 : (date1 > date2);
}


/**
 * valent_sms_store_get_message:
 * @store: a #ValentSmsStore
 * @message_id: a message id
 *
 * Get the #ValentMessage with @message_id or %NULL if not found.
 *
 * Returns: (transfer none) (nullable): a #ValentSmsMessage
 */
ValentSmsMessage *
valent_sms_store_get_message (ValentSmsStore *store,
                              gint64          message_id)
{
  g_autoptr (ValentSqlStmt) stmt = NULL;

  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);

  /* Try loading the message from the database */
  stmt = valent_sql_db_prepare (store->db, GET_MESSAGE_SQL, NULL);

  if (stmt == NULL)
    return NULL;

  valent_sql_stmt_set_int64 (stmt, 1, message_id);

  if (valent_sql_db_step (store->db, stmt, NULL) != VALENT_SQL_STEP_ROW)
    return NULL;

  return deserialize_message (stmt);
}

/**
 * valent_sms_store_get_summary:
 * @store: a #ValentSmsStore
 *
 * Get the latest message of each thread as a #GListModel.
 *
 * Returns: (transfer full) (nullable): a #GListModel
 */
GListModel *
valent_sms_store_get_summary (ValentSmsStore *store)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (ValentSqlStmt) stmt = NULL;

  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);
  g_return_val_if_fail (check_db (store, NULL), NULL);

  if (store->summary)
    return g_object_ref (G_LIST_MODEL (store->summary));

  store->summary = g_list_store_new (VALENT_TYPE_SMS_MESSAGE);
  g_object_add_weak_pointer (G_OBJECT (store->summary),
                             (gpointer)&store->summary);

  /* Query the database */
  stmt = valent_sql_db_prepare (store->db, GET_SUMMARY_SQL, &error);

  if (stmt == NULL)
    return G_LIST_MODEL (store->summary);

  while (valent_sql_db_step (store->db, stmt, &error) == VALENT_SQL_STEP_ROW)
    {
      g_autoptr (ValentSmsMessage) message = NULL;

      message = deserialize_message (stmt);

      if (message != NULL)
        g_list_store_append (store->summary, message);
    }

  return G_LIST_MODEL (store->summary);
}

/**
 * valent_sms_store_dup_thread:
 * @store: a #ValentSmsStore
 * @thread_id: a thread id
 *
 * Get all the messages in @thread_id as a #GQueue.
 *
 * Returns: (transfer container) (nullable): a #GQueue
 */
GQueue *
valent_sms_store_dup_thread (ValentSmsStore *store,
                             gint64          thread_id)
{
  GQueue *thread;

  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);
  g_return_val_if_fail (check_db (store, NULL), NULL);

  thread = valent_sms_store_get_thread (store, thread_id);

  return (thread != NULL) ? g_queue_copy (thread) : NULL;
}

/**
 * valent_sms_store_get_thread:
 * @store: a #ValentSmsStore
 * @thread_id: a message id
 *
 * Get all the messages in @thread_id as a #GQueue.
 *
 * Returns: (transfer none) (nullable): a #GQueue
 */
GQueue *
valent_sms_store_get_thread (ValentSmsStore *store,
                             gint64          thread_id)
{
  GQueue *thread;
  g_autoptr (GError) error = NULL;
  g_autoptr (ValentSqlStmt) stmt = NULL;

  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);
  g_return_val_if_fail (thread_id > 0, NULL);
  g_return_val_if_fail (check_db (store, NULL), NULL);

  thread = g_queue_new ();

  /* Get message ids */
  if (!check_db (store, NULL))
    return thread;

  if ((stmt = valent_sql_db_prepare (store->db, GET_THREAD_SQL, NULL)) == NULL)
    return thread;

  valent_sql_stmt_set_int64 (stmt, 1, thread_id);

  while (valent_sql_db_step (store->db, stmt, &error) == VALENT_SQL_STEP_ROW)
    {
      ValentSmsMessage *message;

      message = deserialize_message (stmt);

      if (message != NULL)
        g_queue_push_tail (thread, message);
    }

  return thread;
}

/**
 * valent_sms_store_get_thread_date:
 * @store: a #ValentSmsStore
 * @thread_id: a thread ID
 *
 * Get the date of the last message in @thread_id.
 *
 * Returns: a UNIX epoch timestamp.
 */
gint64
valent_sms_store_get_thread_date (ValentSmsStore *store,
                                  gint64          thread_id)
{
  g_autoptr (ValentSqlStmt) stmt = NULL;

  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), 0);
  g_return_val_if_fail (thread_id > 0, 0);
  g_return_val_if_fail (check_db (store, NULL), 0);

  stmt = valent_sql_db_prepare (store->db, GET_THREAD_DATE_SQL, NULL);

  if (stmt == NULL)
    return 0;

  valent_sql_stmt_set_int64 (stmt, 1, thread_id);

  if (valent_sql_db_step (store->db, stmt, NULL) != VALENT_SQL_STEP_ROW)
    return 0;

  return valent_sql_stmt_get_int64 (stmt, 0);
}

