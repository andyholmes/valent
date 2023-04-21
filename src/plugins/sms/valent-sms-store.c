// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-store"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>
#include <sqlite3.h>

#include "valent-message.h"
#include "valent-message-thread.h"
#include "valent-sms-store.h"
#include "valent-sms-store-private.h"

/* Ensure that sqlite3_int64 is the same size as int64_t */
G_STATIC_ASSERT (sizeof (sqlite3_int64) == sizeof (int64_t));

struct _ValentSmsStore
{
  ValentContext    parent_instance;

  GAsyncQueue     *queue;
  sqlite3         *connection;
  char            *path;
  sqlite3_stmt    *stmts[9];

  GListStore      *summary;
};

G_DEFINE_FINAL_TYPE (ValentSmsStore, valent_sms_store, VALENT_TYPE_CONTEXT)

enum {
  MESSAGE_ADDED,
  MESSAGE_CHANGED,
  MESSAGE_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

enum {
  STMT_ADD_MESSAGE,
  STMT_REMOVE_MESSAGE,
  STMT_REMOVE_THREAD,
  STMT_GET_MESSAGE,
  STMT_GET_THREAD,
  STMT_GET_THREAD_DATE,
  STMT_GET_THREAD_ITEMS,
  STMT_FIND_MESSAGES,
  STMT_GET_SUMMARY,
  N_STATEMENTS,
};

static char *statements[N_STATEMENTS] = { NULL, };


/*
 * Signal Emission Helpers
 */
typedef struct
{
  GRecMutex       mutex;
  GWeakRef        store;
  ValentMessage  *message;
  guint           signal_id;
} ChangeEmission;

static gboolean
emit_change_main (gpointer data)
{
  ChangeEmission *emission = data;
  g_autoptr (ValentSmsStore) store = NULL;

  g_assert (emission != NULL);

  g_rec_mutex_lock (&emission->mutex);
  if ((store = g_weak_ref_get (&emission->store)) != NULL)
    {
      g_signal_emit (G_OBJECT (store),
                     emission->signal_id, 0,
                     emission->message);
    }

  g_weak_ref_clear (&emission->store);
  g_clear_object (&emission->message);
  g_rec_mutex_unlock (&emission->mutex);
  g_rec_mutex_clear (&emission->mutex);
  g_clear_pointer (&emission, g_free);

  return G_SOURCE_REMOVE;
}


/*
 * sqlite Threading Helpers
 */
enum {
  TASK_DEFAULT,
  TASK_CRITICAL,
  TASK_TERMINAL,
};

typedef struct
{
  GTask           *task;
  GTaskThreadFunc  task_func;
  unsigned int     task_mode;
} TaskClosure;

static void
task_closure_free (gpointer data)
{
  g_autofree TaskClosure *closure = data;

  g_clear_object (&closure->task);
  g_clear_pointer (&closure, g_free);
}

static void
task_closure_cancel (gpointer data)
{
  g_autofree TaskClosure *closure = data;

  if (G_IS_TASK (closure->task) && !g_task_get_completed (closure->task))
    {
      g_task_return_new_error (closure->task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "Operation cancelled");
    }

  g_clear_pointer (&closure, task_closure_free);
}

static gpointer
valent_sms_store_thread (gpointer data)
{
  g_autoptr (GAsyncQueue) tasks = data;
  TaskClosure *closure = NULL;

  while ((closure = g_async_queue_pop (tasks)))
    {
      unsigned int mode = closure->task_mode;

      if (G_IS_TASK (closure->task) && !g_task_get_completed (closure->task))
        {
          closure->task_func (closure->task,
                              g_task_get_source_object (closure->task),
                              g_task_get_task_data (closure->task),
                              g_task_get_cancellable (closure->task));

          if (mode == TASK_CRITICAL && g_task_had_error (closure->task))
            mode = TASK_TERMINAL;
        }

      g_clear_pointer (&closure, task_closure_free);

      if (mode == TASK_TERMINAL)
        break;
    }

  /* Cancel any queued tasks */
  g_async_queue_lock (tasks);

  while ((closure = g_async_queue_try_pop_unlocked (tasks)) != NULL)
    g_clear_pointer (&closure, task_closure_cancel);

  g_async_queue_unlock (tasks);

  return NULL;
}

/*
 * Step functions
 */
static inline ValentMessage *
valent_sms_store_get_message_step (sqlite3_stmt  *stmt,
                                   GError       **error)
{
  g_autoptr (GVariant) metadata = NULL;
  const char *metadata_str;
  int rc;

  g_assert (stmt != NULL);
  g_assert (error == NULL || *error == NULL);

  if ((rc = sqlite3_step (stmt)) == SQLITE_DONE)
    return NULL;

  if (rc != SQLITE_ROW)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s: %s", G_STRFUNC, sqlite3_errstr (rc));
      return NULL;
    }

  if ((metadata_str = (const char *)sqlite3_column_text (stmt, 3)) != NULL)
    metadata = g_variant_parse (NULL, metadata_str, NULL, NULL, NULL);

  return g_object_new (VALENT_TYPE_MESSAGE,
                       "box",       sqlite3_column_int (stmt, 0),
                       "date",      sqlite3_column_int64 (stmt, 1),
                       "id",        sqlite3_column_int64 (stmt, 2),
                       "metadata",  metadata,
                       "read",      sqlite3_column_int (stmt, 4),
                       "sender",    sqlite3_column_text (stmt, 5),
                       "text",      sqlite3_column_text (stmt, 6),
                       "thread_id", sqlite3_column_int64 (stmt, 7),
                       NULL);
}

static inline gboolean
valent_sms_store_set_message_step (sqlite3_stmt   *stmt,
                                   ValentMessage  *message,
                                   GError        **error)
{
  int rc;
  ValentMessageBox box;
  int64_t date;
  int64_t id;
  GVariant *metadata;
  gboolean read;
  const char *sender;
  const char *text;
  int64_t thread_id;
  g_autofree char *metadata_str = NULL;

  /* Extract the message data */
  box = valent_message_get_box (message);
  date = valent_message_get_date (message);
  id = valent_message_get_id (message);
  metadata = valent_message_get_metadata (message);
  read = valent_message_get_read (message);
  sender = valent_message_get_sender (message);
  text = valent_message_get_text (message);
  thread_id = valent_message_get_thread_id (message);

  if (metadata != NULL)
    metadata_str = g_variant_print (metadata, TRUE);

  /* Bind the message data */
  sqlite3_bind_int (stmt, 1, box);
  sqlite3_bind_int64 (stmt, 2, date);
  sqlite3_bind_int64 (stmt, 3, id);
  sqlite3_bind_text (stmt, 4, metadata_str, -1, NULL);
  sqlite3_bind_int (stmt, 5, read);
  sqlite3_bind_text (stmt, 6, sender, -1, NULL);
  sqlite3_bind_text (stmt, 7, text, -1, NULL);
  sqlite3_bind_int64 (stmt, 8, thread_id);

  /* Execute and auto-reset */
  if ((rc = sqlite3_step (stmt)) != SQLITE_DONE)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "%s: %s", G_STRFUNC, sqlite3_errstr (rc));
      sqlite3_reset (stmt);
      return FALSE;
    }

  sqlite3_reset (stmt);
  return TRUE;
}

static gboolean
valent_sms_store_return_error_if_closed (GTask          *task,
                                         ValentSmsStore *self)
{
  g_assert (G_IS_TASK (task));
  g_assert (VALENT_IS_SMS_STORE (self));

  if G_UNLIKELY (self->connection == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CONNECTION_CLOSED,
                               "Database connection closed");
      return TRUE;
    }

  return FALSE;
}


/*
 * Database Hooks
 */
static void
update_hook (gpointer       user_data,
             int            event,
             char const    *database,
             char const    *table,
             sqlite3_int64  rowid)
{
  ValentSmsStore *self = VALENT_SMS_STORE (user_data);
  sqlite3_stmt *stmt = self->stmts[STMT_GET_MESSAGE];
  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_SMS_STORE (self));
  g_assert (!VALENT_IS_MAIN_THREAD ());

  if G_UNLIKELY (g_strcmp0 (table, "message") != 0)
    return;

  if (event != SQLITE_DELETE)
    {
      sqlite3_bind_int64 (stmt, 1, rowid);
      message = valent_sms_store_get_message_step (stmt, &error);
      sqlite3_reset (stmt);
    }

  if G_UNLIKELY (error != NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  /* Fallback to using a message skeleton */
  if (message == NULL)
    {
      message = g_object_new (VALENT_TYPE_MESSAGE,
                              "id", rowid,
                              NULL);
    }

  switch (event)
    {
    case SQLITE_INSERT:
      valent_sms_store_message_added (self, message);
      break;

    case SQLITE_UPDATE:
      valent_sms_store_message_changed (self, message);
      break;

    case SQLITE_DELETE:
      valent_sms_store_message_removed (self, message);
      break;
    }
}


/*
 * ValentSmsStore Tasks
 */
static void
valent_sms_store_open_task (GTask        *task,
                            gpointer      source_object,
                            gpointer      task_data,
                            GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  const char *path = task_data;
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (self->connection != NULL)
    return g_task_return_boolean (task, TRUE);

  /* Pass NOMUTEX since concurrency is managed by the GMutex*/
  rc = sqlite3_open_v2 (path,
                        &self->connection,
                        (SQLITE_OPEN_READWRITE |
                         SQLITE_OPEN_CREATE |
                         SQLITE_OPEN_NOMUTEX),
                        NULL);

  if (rc != SQLITE_OK)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "sqlite3_open_v2(): \"%s\": [%i] %s",
                               path, rc, sqlite3_errstr (rc));
      g_clear_pointer (&self->connection, sqlite3_close);
      return;
    }

  /* Prepare the tables */
  rc = sqlite3_exec (self->connection,
                     MESSAGE_TABLE_SQL,
                     NULL,
                     NULL,
                     NULL);

  if (rc != SQLITE_OK)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "sqlite3_prepare_v2(): [%i] \"message\" Table: %s",
                               rc, sqlite3_errstr (rc));
      g_clear_pointer (&self->connection, sqlite3_close);
      return;
    }

  /* Prepare the statements */
  for (unsigned int i = 0; i < N_STATEMENTS; i++)
    {
      sqlite3_stmt *stmt = NULL;
      const char *sql = statements[i];

      rc = sqlite3_prepare_v2 (self->connection, sql, -1, &stmt, NULL);

      if (rc != SQLITE_OK)
        {
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   "sqlite3_prepare_v2(): \"%s\": [%i] %s",
                                   sql, rc, sqlite3_errstr (rc));
          g_clear_pointer (&self->connection, sqlite3_close);
          return;
        }

      self->stmts[i] = g_steal_pointer (&stmt);
    }

  /* Connect the hooks */
  sqlite3_update_hook (self->connection, update_hook, self);

  g_task_return_boolean (task, TRUE);
}

static void
valent_sms_store_close_task (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (self->connection == NULL)
    return g_task_return_boolean (task, TRUE);

  /* Cleanup cached statements */
  for (unsigned int i = 0; i < N_STATEMENTS; i++)
    g_clear_pointer (&self->stmts[i], sqlite3_finalize);

  /* Optimize the database before closing.
   *
   * See:
   *   https://www.sqlite.org/pragma.html#pragma_optimize
   *   https://www.sqlite.org/queryplanner-ng.html#update_2017_a_better_fix
   */
  rc = sqlite3_exec (self->connection, "PRAGMA optimize;", NULL, NULL, NULL);

  if (rc != SQLITE_OK)
    {
      g_debug ("sqlite3_exec(): \"%s\": [%i] %s",
               "PRAGMA optimize;", rc, sqlite3_errstr (rc));
    }

  /* Close the connection */
  if ((rc = sqlite3_close (self->connection)) != SQLITE_OK)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "sqlite3_close(): [%i] %s",
                               rc, sqlite3_errstr (rc));
      return;
    }

  self->connection = NULL;
  g_task_return_boolean (task, TRUE);
}

static void
add_messages_task (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  GPtrArray *messages = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_ADD_MESSAGE];
  unsigned int n_messages = messages->len;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  for (unsigned int i = 0; i < n_messages; i++)
    {
      ValentMessage *message = g_ptr_array_index (messages, i);

      /* Iterate the results stopping on error to mark the point of failure */
      if (!valent_sms_store_set_message_step (stmt, message, &error))
        {
          n_messages = i;
          break;
        }
    }

  /* Truncate the input on failure, since we'll be emitting signals */
  if (n_messages < messages->len)
    g_ptr_array_remove_range (messages, n_messages, messages->len - n_messages);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_boolean (task, TRUE);
}

static void
remove_message_task (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  int64_t *message_id = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_REMOVE_MESSAGE];
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  sqlite3_bind_int64 (stmt, 1, *message_id);
  rc = sqlite3_step (stmt);
  sqlite3_reset (stmt);

  if (rc == SQLITE_DONE || rc == SQLITE_OK)
    return g_task_return_boolean (task, TRUE);

  return g_task_return_new_error (task,
                                  G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "%s: %s",
                                  G_STRFUNC, sqlite3_errstr (rc));
}

static void
remove_thread_task (GTask        *task,
                      gpointer      source_object,
                      gpointer      task_data,
                      GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  int64_t *thread_id = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_REMOVE_THREAD];
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  sqlite3_bind_int64 (stmt, 1, *thread_id);
  rc = sqlite3_step (stmt);
  sqlite3_reset (stmt);

  if (rc == SQLITE_DONE || rc == SQLITE_OK)
    return g_task_return_boolean (task, TRUE);

  return g_task_return_new_error (task,
                                  G_IO_ERROR,
                                  G_IO_ERROR_FAILED,
                                  "%s: %s",
                                  G_STRFUNC, sqlite3_errstr (rc));
}

static void
find_messages_task (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  const char *query = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_FIND_MESSAGES];
  g_autoptr (GPtrArray) messages = NULL;
  g_autofree char *query_param = NULL;
  ValentMessage *message;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  // NOTE: escaped percent signs (%%) are query wildcards (%)
  query_param = g_strdup_printf ("%%%s%%", query);
  sqlite3_bind_text (stmt, 1, query_param, -1, NULL);

  /* Collect the results */
  messages = g_ptr_array_new_with_free_func (g_object_unref);

  while ((message = valent_sms_store_get_message_step (stmt, &error)))
    g_ptr_array_add (messages, message);
  sqlite3_reset (stmt);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task,
                         g_steal_pointer (&messages),
                         (GDestroyNotify)g_ptr_array_unref);
}

static void
get_message_task (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  int64_t *message_id = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_GET_MESSAGE];
  g_autoptr (ValentMessage) message = NULL;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  sqlite3_bind_int64 (stmt, 1, *message_id);
  message = valent_sms_store_get_message_step (stmt, &error);
  sqlite3_reset (stmt);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&message), g_object_unref);
}

static void
get_summary_cb (ValentSmsStore *self,
                GAsyncResult   *result,
                gpointer        user_data)
{
  g_autoptr (GPtrArray) messages = NULL;
  g_autoptr (GError) error = NULL;

  if ((messages = g_task_propagate_pointer (G_TASK (result), &error)) == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  g_list_store_splice (self->summary, 0, 0, messages->pdata, messages->len);
}

static void
get_summary_task (GTask        *task,
                  gpointer      source_object,
                  gpointer      task_data,
                  GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  sqlite3_stmt *stmt = self->stmts[STMT_GET_SUMMARY];
  g_autoptr (GPtrArray) messages = NULL;
  ValentMessage *message;
  GError *error = NULL;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  /* Collect the results */
  messages = g_ptr_array_new_with_free_func (g_object_unref);

  while ((message = valent_sms_store_get_message_step (stmt, &error)))
    g_ptr_array_add (messages, message);
  sqlite3_reset (stmt);

  if (error != NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task,
                         g_steal_pointer (&messages),
                         (GDestroyNotify)g_ptr_array_unref);
}

static void
get_thread_date_task (GTask        *task,
                      gpointer      source_object,
                      gpointer      task_data,
                      GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  int64_t *thread_id = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_GET_THREAD_DATE];
  int64_t date = 0;
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  sqlite3_bind_int64 (stmt, 1, *thread_id);

  if ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    date = sqlite3_column_int64 (stmt, 0);

  sqlite3_reset (stmt);

  if (rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "%s: %s",
                               G_STRFUNC, sqlite3_errstr (rc));
      return;
    }

  g_task_return_int (task, date);
}

static void
get_thread_items_task (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  ValentSmsStore *self = VALENT_SMS_STORE (source_object);
  int64_t *thread_id = task_data;
  sqlite3_stmt *stmt = self->stmts[STMT_GET_THREAD_ITEMS];
  g_autoptr (GPtrArray) messages = NULL;
  int rc;

  if (g_task_return_error_if_cancelled (task))
    return;

  if (valent_sms_store_return_error_if_closed (task, self))
    return;

  messages = g_ptr_array_new_with_free_func (g_object_unref);
  sqlite3_bind_int64 (stmt, 1, *thread_id);

  while ((rc = sqlite3_step (stmt)) == SQLITE_ROW)
    {
      ValentMessage *message;

      message = g_object_new (VALENT_TYPE_MESSAGE,
                              "date",      sqlite3_column_int64 (stmt, 0),
                              "id",        sqlite3_column_int64 (stmt, 1),
                              "sender",    sqlite3_column_text (stmt, 2),
                              "thread-id", *thread_id,
                              NULL);
      g_ptr_array_add (messages, message);
    }

  sqlite3_reset (stmt);

  if (rc != SQLITE_DONE && rc != SQLITE_ROW)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "%s: %s",
                               G_STRFUNC, sqlite3_errstr (rc));
      return;
    }

  g_task_return_pointer (task,
                         g_steal_pointer (&messages),
                         (GDestroyNotify)g_ptr_array_unref);
}


/*
 * Private
 */
static inline void
valent_sms_store_push (ValentSmsStore  *self,
                       GTask           *task,
                       GTaskThreadFunc  task_func)
{
  TaskClosure *closure = NULL;

  if G_UNLIKELY (self->queue == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_CLOSED,
                               "Store is closed");
      return;
    }

  closure = g_new0 (TaskClosure, 1);
  closure->task = g_object_ref (task);
  closure->task_func = task_func;
  closure->task_mode = TASK_DEFAULT;
  g_async_queue_push (self->queue, closure);
}

static void
valent_sms_store_open (ValentSmsStore *self)
{
  g_autoptr (GThread) thread = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GTask) task = NULL;
  g_autoptr (GFile) file = NULL;

  file = valent_context_get_cache_file (VALENT_CONTEXT (self), "sms.db");
  self->path = g_file_get_path (file);

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, valent_sms_store_open);
  g_task_set_task_data (task, g_strdup (self->path), g_free);
  valent_sms_store_push (self, task, valent_sms_store_open_task);

  /* Spawn the worker thread, passing in a reference to the queue */
  thread = g_thread_try_new ("valent-task-queue",
                             valent_sms_store_thread,
                             g_async_queue_ref (self->queue),
                             &error);

  /* On failure drop the reference passed to the thread, then clear the last
   * reference so the open task is cancelled and new tasks are rejected */
  if (error != NULL)
    {
      g_critical ("%s: Failed to spawn worker thread: %s",
                  G_OBJECT_TYPE_NAME (self),
                  error->message);
      g_async_queue_unref (self->queue);
      g_clear_pointer (&self->queue, g_async_queue_unref);
    }
}

static void
valent_sms_store_close (ValentSmsStore *self)
{
  g_autoptr (GTask) task = NULL;
  TaskClosure *closure = NULL;

  task = g_task_new (self, NULL, NULL, NULL);
  g_task_set_source_tag (task, valent_sms_store_close);

  closure = g_new0 (TaskClosure, 1);
  closure->task = g_object_ref (task);
  closure->task_func = valent_sms_store_close_task;
  closure->task_mode = TASK_TERMINAL;
  g_async_queue_push (self->queue, closure);
}


/*
 * GObject
 */
static void
valent_sms_store_constructed (GObject *object)
{
  ValentSmsStore *self = VALENT_SMS_STORE (object);

  /* Chain-up before queueing the open task to ensure the path is prepared */
  G_OBJECT_CLASS (valent_sms_store_parent_class)->constructed (object);

  valent_sms_store_open (self);
}

static void
valent_sms_store_dispose (GObject *object)
{
  ValentSmsStore *self = VALENT_SMS_STORE (object);

  /* We will drop our reference to queue once we queue the closing task, then
   * the task itself will end up holding the last reference. */
  if (self->queue != NULL)
    {
      valent_sms_store_close (self);
      g_clear_pointer (&self->queue, g_async_queue_unref);
    }

  G_OBJECT_CLASS (valent_sms_store_parent_class)->dispose (object);
}

static void
valent_sms_store_finalize (GObject *object)
{
  ValentSmsStore *self = VALENT_SMS_STORE (object);

  g_clear_pointer (&self->queue, g_async_queue_unref);
  g_clear_pointer (&self->path, g_free);
  g_clear_weak_pointer (&self->summary);

  G_OBJECT_CLASS (valent_sms_store_parent_class)->finalize (object);
}

static void
valent_sms_store_class_init (ValentSmsStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_sms_store_constructed;
  object_class->dispose = valent_sms_store_dispose;
  object_class->finalize = valent_sms_store_finalize;

  /**
   * ValentSmsStore::message-added:
   * @store: a #ValentSmsStore
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
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MESSAGE);
  g_signal_set_va_marshaller (signals [MESSAGE_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentSmsStore::message-changed:
   * @store: a #ValentSmsStore
   * @message: a #ValentMessage
   *
   * ValentSmsStore::message-changed is emitted when a message is updated in
   * @store.
   */
  signals [MESSAGE_CHANGED] =
    g_signal_new ("message-changed",
                  VALENT_TYPE_SMS_STORE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MESSAGE);
  g_signal_set_va_marshaller (signals [MESSAGE_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentSmsStore::message-removed:
   * @store: a #ValentSmsStore
   * @message: a #ValentMessage
   *
   * ValentSmsStore::message-removed is emitted when a message is removed from
   * @store.
   */
  signals [MESSAGE_REMOVED] =
    g_signal_new ("message-removed",
                  VALENT_TYPE_SMS_STORE,
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MESSAGE);
  g_signal_set_va_marshaller (signals [MESSAGE_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /* SQL Statements */
  statements[STMT_ADD_MESSAGE] = ADD_MESSAGE_SQL;
  statements[STMT_REMOVE_MESSAGE] = REMOVE_MESSAGE_SQL;
  statements[STMT_REMOVE_THREAD] = REMOVE_THREAD_SQL;
  statements[STMT_GET_MESSAGE] = GET_MESSAGE_SQL;
  statements[STMT_GET_THREAD] = GET_THREAD_SQL;
  statements[STMT_GET_THREAD_DATE] = GET_THREAD_DATE_SQL;
  statements[STMT_GET_THREAD_ITEMS] = GET_THREAD_ITEMS_SQL;
  statements[STMT_FIND_MESSAGES] = FIND_MESSAGES_SQL;
  statements[STMT_GET_SUMMARY] = GET_SUMMARY_SQL;
}

static void
valent_sms_store_init (ValentSmsStore *self)
{
  self->queue = g_async_queue_new_full (task_closure_cancel);
}

/**
 * valent_sms_store_new:
 * @parent: a #ValentContext
 *
 * Create a new #ValentSmsStore.
 *
 * Returns: (transfer full): a new sms store
 */
ValentSmsStore *
valent_sms_store_new (ValentContext *parent)
{
  return g_object_new (VALENT_TYPE_SMS_STORE,
                       "domain", "plugin",
                       "id",     "sms",
                       "parent", parent,
                       NULL);
}

/**
 * valent_sms_store_add_message:
 * @store: a #ValentSmsStore
 * @message: a #ValentMessage
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Add @message to @store.
 */
void
valent_sms_store_add_message (ValentSmsStore      *store,
                              ValentMessage       *message,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GPtrArray) messages = NULL;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  messages = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (messages, g_object_ref (message));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_sms_store_add_message);
  g_task_set_task_data (task,
                        g_steal_pointer (&messages),
                        (GDestroyNotify)g_ptr_array_unref);
  valent_sms_store_push (store, task, add_messages_task);
}

/**
 * valent_sms_store_add_messages:
 * @store: a #ValentSmsStore
 * @messages: (element-type Valent.Message): a #ValentMessage
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Add @messages to @store.
 */
void
valent_sms_store_add_messages (ValentSmsStore      *store,
                               GPtrArray           *messages,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (messages != NULL);

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_sms_store_add_message);
  g_task_set_task_data (task,
                        g_ptr_array_ref (messages),
                        (GDestroyNotify)g_ptr_array_unref);
  valent_sms_store_push (store, task, add_messages_task);
}

/**
 * valent_sms_store_add_messages_finish:
 * @store: a #ValentSmsStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_sms_store_add_messages().
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_sms_store_add_messages_finish (ValentSmsStore  *store,
                                      GAsyncResult    *result,
                                      GError         **error)
{
  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_sms_store_remove_message:
 * @store: a #ValentSmsStore
 * @message_id: a message ID
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Remove the message with @message_id from @thread_id.
 */
void
valent_sms_store_remove_message (ValentSmsStore      *store,
                                 int64_t              message_id,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  int64_t *task_data;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));

  task_data = g_new0 (int64_t, 1);
  *task_data = message_id;

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_task_data (task, task_data, g_free);
  g_task_set_source_tag (task, valent_sms_store_remove_message);
  valent_sms_store_push (store, task, remove_message_task);
}

/**
 * valent_sms_store_remove_message_finish:
 * @store: a #ValentSmsStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_sms_store_remove_message().
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_sms_store_remove_message_finish (ValentSmsStore  *store,
                                        GAsyncResult    *result,
                                        GError         **error)
{
  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_sms_store_remove_thread:
 * @store: a #ValentSmsStore
 * @thread_id: a thread ID
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Remove @thread_id and all it's messages from @store.
 */
void
valent_sms_store_remove_thread (ValentSmsStore      *store,
                                int64_t              thread_id,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  int64_t *task_data;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (thread_id >= 0);

  task_data = g_new0 (int64_t, 1);
  *task_data = thread_id;

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_task_data (task, task_data, g_free);
  g_task_set_source_tag (task, valent_sms_store_remove_thread);
  valent_sms_store_push (store, task, remove_thread_task);
}

/**
 * valent_sms_store_remove_thread_finish:
 * @store: a #ValentSmsStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_sms_store_remove_thread().
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
gboolean
valent_sms_store_remove_thread_finish (ValentSmsStore  *store,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * valent_sms_store_find_messages:
 * @store: a #ValentSmsStore
 * @query: a string to search for
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Search through all the messages in @store and return the most recent message
 * from each thread containing @query.
 *
 * Call valent_sms_store_find_messages_finish() to get the result.
 */
void
valent_sms_store_find_messages (ValentSmsStore      *store,
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
  g_task_set_source_tag (task, valent_sms_store_find_messages);
  g_task_set_task_data (task, g_strdup (query), g_free);
  valent_sms_store_push (store, task, find_messages_task);
}

/**
 * valent_sms_store_find_messages_finish:
 * @store: a #ValentSmsStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_sms_store_find_messages().
 *
 * Returns: (transfer container) (element-type Valent.Message): an #GPtrArray
 */
GPtrArray *
valent_sms_store_find_messages_finish (ValentSmsStore  *store,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);
  g_return_val_if_fail (g_task_is_valid (result, store), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * valent_sms_store_get_message:
 * @store: a #ValentSmsStore
 * @message_id: a message ID
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Get the #ValentMessage with @message_id or %NULL if not found.
 *
 * Returns: (transfer none) (nullable): a #ValentMessage
 */
void
valent_sms_store_get_message (ValentSmsStore      *store,
                              int64_t              message_id,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  int64_t *task_data;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task_data = g_new (int64_t, 1);
  *task_data = message_id;

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_task_data (task, task_data, g_free);
  g_task_set_source_tag (task, valent_sms_store_get_message);
  valent_sms_store_push (store, task, get_message_task);
}

/**
 * valent_sms_store_get_message_finish:
 * @store: a #ValentSmsStore
 * @result: a #GAsyncResult
 * @error: (nullable): a #GError
 *
 * Finish an operation started by valent_sms_store_get_message().
 *
 * Returns: (transfer full) (nullable): a #ValentMessage
 */
ValentMessage *
valent_sms_store_get_message_finish (ValentSmsStore  *store,
                                     GAsyncResult    *result,
                                     GError         **error)
{
  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);
  g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_pointer (G_TASK (result), error);
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
  g_autoptr (GTask) task = NULL;

  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);

  if (store->summary != NULL)
    return g_object_ref (G_LIST_MODEL (store->summary));

  store->summary = g_list_store_new (VALENT_TYPE_MESSAGE);
  g_object_add_weak_pointer (G_OBJECT (store->summary),
                             (gpointer)&store->summary);

  task = g_task_new (store, NULL, (GAsyncReadyCallback)get_summary_cb, NULL);
  g_task_set_source_tag (task, valent_sms_store_get_summary);
  valent_sms_store_push (store, task, get_summary_task);

  return G_LIST_MODEL (store->summary);
}

/**
 * valent_sms_store_get_thread:
 * @store: a #ValentSmsStore
 * @thread_id: a message id
 *
 * Get the thread with @thread_id as a #GListModel.
 *
 * Returns: (transfer full): a #GListModel
 */
GListModel *
valent_sms_store_get_thread (ValentSmsStore *store,
                             int64_t         thread_id)
{
  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), 0);
  g_return_val_if_fail (thread_id > 0, 0);

  return valent_message_thread_new (store, thread_id);
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
int64_t
valent_sms_store_get_thread_date (ValentSmsStore *store,
                                  int64_t         thread_id)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GError) error = NULL;
  int64_t date = 0;

  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), 0);
  g_return_val_if_fail (thread_id >= 0, 0);

  task = g_task_new (store, NULL, NULL, NULL);
  g_task_set_source_tag (task, valent_sms_store_get_thread_date);
  g_task_set_task_data (task, &thread_id, NULL);
  valent_sms_store_push (store, task, get_thread_date_task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (NULL, FALSE);

  date = g_task_propagate_int (task, &error);

  if (error != NULL)
    g_warning ("%s(): %s", G_STRFUNC, error->message);

  return date;
}

/**
 * valent_sms_store_get_thread_items:
 * @store: a #ValentSmsStore
 * @thread_id: a thread ID
 *
 * Get the #ValentMessage in @thread_id at @position, when sorted by date in
 * ascending order.
 */
void
valent_sms_store_get_thread_items (ValentSmsStore      *store,
                                   int64_t              thread_id,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  int64_t *task_data;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (thread_id >= 0);

  task_data = g_new0 (int64_t, 1);
  *task_data = thread_id;

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_sms_store_get_thread_items);
  g_task_set_task_data (task, task_data, g_free);
  valent_sms_store_push (store, task, get_thread_items_task);
}

/**
 * valent_sms_store_message_added:
 * @store: a #ValentSmsStore
 * @message: a #ValentMessage
 *
 * Emits the #ValentSmsStore::message-added signal on @store.
 *
 * This function should only be called by classes implementing
 * #ValentSmsStore. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_sms_store_message_added (ValentSmsStore *store,
                                ValentMessage  *message)
{
  ChangeEmission *emission;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (store), signals [MESSAGE_ADDED], 0, message);
      return;
    }

  emission = g_new0 (ChangeEmission, 1);
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->store, store);
  emission->message = g_object_ref (message);
  emission->signal_id = signals [MESSAGE_ADDED];
  g_rec_mutex_unlock (&emission->mutex);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   emit_change_main,
                   g_steal_pointer (&emission),
                   NULL);
}

/**
 * valent_sms_store_message_removed:
 * @store: a #ValentSmsStore
 * @message: a #ValentMessage
 *
 * Emits the #ValentSmsStore::message-removed signal on @store.
 *
 * This function should only be called by classes implementing
 * #ValentSmsStore. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_sms_store_message_removed (ValentSmsStore *store,
                                  ValentMessage  *message)
{
  ChangeEmission *emission;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (store), signals [MESSAGE_REMOVED], 0, message);
      return;
    }

  emission = g_new0 (ChangeEmission, 1);
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->store, store);
  emission->message = g_object_ref (message);
  emission->signal_id = signals [MESSAGE_REMOVED];
  g_rec_mutex_unlock (&emission->mutex);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   emit_change_main,
                   g_steal_pointer (&emission),
                   NULL);
}

/**
 * valent_sms_store_message_changed:
 * @store: a #ValentSmsStore
 * @message: a #ValentMessage
 *
 * Emits the #ValentSmsStore::message-changed signal on @store.
 *
 * This function should only be called by classes implementing
 * #ValentSmsStore. It has to be called after the internal representation
 * of @store has been updated, because handlers connected to this signal
 * might query the new state of the provider.
 */
void
valent_sms_store_message_changed (ValentSmsStore *store,
                                  ValentMessage  *message)
{
  ChangeEmission *emission;

  g_return_if_fail (VALENT_IS_SMS_STORE (store));
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_signal_emit (G_OBJECT (store), signals [MESSAGE_CHANGED], 0, message);
      return;
    }

  emission = g_new0 (ChangeEmission, 1);
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->store, store);
  emission->message = g_object_ref (message);
  emission->signal_id = signals [MESSAGE_CHANGED];
  g_rec_mutex_unlock (&emission->mutex);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   emit_change_main,
                   g_steal_pointer (&emission),
                   NULL);
}

