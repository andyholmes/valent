// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sql-db"

#include "config.h"

#include <gio/gio.h>

#include "valent-sql-db.h"


struct _ValentSqlDb
{
  GObject       parent_instance;

  /* sqlite */
  GRecMutex     mutex;
  char         *path;
  sqlite3      *sqldb;
  unsigned int  open : 1;
};

G_DEFINE_TYPE (ValentSqlDb, valent_sql_db, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_OPEN,
  PROP_PATH,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static inline gboolean
valent_sql_db_check (ValentSqlDb  *db,
                     GError      **error)
{
  if G_LIKELY (db->open)
    return TRUE;

  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_CLOSED,
               "sqlite database is closed");
  return FALSE;
}

/*
 * GObject
 */
static void
valent_sql_db_finalize (GObject *object)
{
  ValentSqlDb *self = VALENT_SQL_DB (object);

  g_clear_pointer (&self->path, g_free);

  G_OBJECT_CLASS (valent_sql_db_parent_class)->finalize (object);
}

static void
valent_sql_db_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ValentSqlDb *self = VALENT_SQL_DB (object);

  switch (prop_id)
    {
    case PROP_OPEN:
      g_value_set_boolean (value, self->open);
      break;

    case PROP_PATH:
      g_value_set_string (value, self->path);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sql_db_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ValentSqlDb *self = VALENT_SQL_DB (object);

  switch (prop_id)
    {
    case PROP_PATH:
      self->path = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sql_db_init (ValentSqlDb *message)
{
}

static void
valent_sql_db_class_init (ValentSqlDbClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_sql_db_finalize;
  object_class->get_property = valent_sql_db_get_property;
  object_class->set_property = valent_sql_db_set_property;

  /**
   * ValentSqlDb:open:
   *
   * Whether the #ValentSqlDb is open.
   */
  properties [PROP_OPEN] =
    g_param_spec_boolean ("open",
                          "Open",
                          "Whether the database is open",
                          FALSE,
                          (G_PARAM_READABLE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentSqlDb:path:
   *
   * The path to the `sqlite3` database.
   */
  properties [PROP_PATH] =
    g_param_spec_string ("path",
                         "Path",
                         "The path to the sqlite3 database",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * valent_sql_db_new:
 * @path: (type filename): a path
 *
 * Create a new #ValentSqlDb for @path.
 *
 * Returns: (transfer full): a #ValentSqlDb
 */
ValentSqlDb *
valent_sql_db_new (const char *path)
{
  g_return_val_if_fail (path != NULL, NULL);

  return g_object_new (VALENT_TYPE_SQL_DB,
                       "path", path,
                       NULL);
}

/**
 * valent_sql_db_lock:
 * @db: a #ValentSqlDb
 *
 * Acquires the lock for @db.
 *
 * Call valent_sql_db_unlock() to release the lock.
 *
 * The synchronization used is a #GRecMutex.
 */
void
valent_sql_db_lock (ValentSqlDb *db)
{
  g_return_if_fail (VALENT_IS_SQL_DB (db));

  g_rec_mutex_lock (&db->mutex);
}

/**
 * valent_sql_db_unlock:
 * @db: a #ValentSqlDb
 *
 * Releases a previously acquired lock from valent_sql_db_lock().
 *
 * The synchronization used is a #GRecMutex.
 */
void
valent_sql_db_unlock (ValentSqlDb *db)
{
  g_return_if_fail (VALENT_IS_SQL_DB (db));

  g_rec_mutex_unlock (&db->mutex);
}

/**
 * valent_sql_db_open:
 * @db: a #ValentSqlDb
 * @error: (nullable): a #GError
 *
 * Open the sqlite database for @db, returning %TRUE on success or %FALSE with
 * @error set.
 *
 * Returns: %TRUE on success
 */
gboolean
valent_sql_db_open (ValentSqlDb  *db,
                    GError      **error)
{
  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (db->open)
    return TRUE;

  /* Open sqlite database */
  /* ensure_directory (db->cache_path); */
  /* path = g_build_filename (db->cache_path, "data.db", NULL); */

  if (sqlite3_open (db->path, &db->sqldb) != SQLITE_OK)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   G_FILE_ERROR_FAILED,
                   "Error opening database: %s",
                   sqlite3_errmsg (db->sqldb));
      g_clear_pointer (&db->sqldb, sqlite3_close);

      return FALSE;
    }

  /* Mark as open */
  db->open = TRUE;

  return TRUE;
}

/**
 * valent_sql_db_close:
 * @db: a #ValentSqlDb
 *
 * Close the sqlite database for @db.
 */
void
valent_sql_db_close (ValentSqlDb *db)
{
  g_return_if_fail (VALENT_IS_SQL_DB (db));

  if (!db->open)
    return;

  /* Close sqlite database */
  g_clear_pointer (&db->sqldb, sqlite3_close);
  db->open = FALSE;
}

/**
 * valent_sql_db_begin:
 * @db: a #ValentSqlDb
 *
 * Begin a sqlite transaction with @db.
 *
 * Returns: %TRUE if the transaction has begun
 */
gboolean
valent_sql_db_begin (ValentSqlDb *db)
{
  char *sqlerr;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);

  if (!valent_sql_db_check (db, NULL))
    return FALSE;

  if (sqlite3_exec (db->sqldb, "BEGIN TRANSACTION;", 0, 0, &sqlerr) != SQLITE_OK)
    {
      g_warning ("Error beginning transaction: %s", sqlerr);
      g_clear_pointer (&sqlerr, sqlite3_free);
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_sql_db_commit:
 * @db: a #ValentSqlDb
 *
 * Commit a sqlite transaction with @db previously started with
 * valent_data_begin().
 *
 * Returns: %TRUE if the transaction was committed successfully
 */
gboolean
valent_sql_db_commit (ValentSqlDb *db)
{
  char *sqlerr;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);

  if (!valent_sql_db_check (db, NULL))
    return FALSE;

  if (sqlite3_exec (db->sqldb, "COMMIT;", 0, 0, &sqlerr) != SQLITE_OK)
    {
      g_warning ("Error committing transaction: %s", sqlerr);
      g_clear_pointer (&sqlerr, sqlite3_free);
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_sql_db_exec:
 * @db: a #ValentSqlDb
 * @sql: a SQL query
 * @error: (nullable): a #GError
 *
 * Execute a simple SQL statement, returning %TRUE on success or %FALSE with
 * @error set.
 *
 * Returns: %TRUE if successful.
 */
gboolean
valent_sql_db_exec (ValentSqlDb  *db,
                      const char  *sql,
                      GError     **error)
{
  char *sqlerr;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);
  g_return_val_if_fail (sql != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!valent_sql_db_check (db, error))
    return FALSE;

  if (sqlite3_exec (db->sqldb, sql, 0, 0, &sqlerr) != SQLITE_OK)
    {
      g_set_error (error, G_FILE_ERROR,
                   G_FILE_ERROR_FAILED,
                   "Error executing '%s': %s",
                   sql, sqlerr);
      g_clear_pointer (&sqlerr, sqlite3_free);
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_sql_db_prepare:
 * @db: a #ValentSqlDb
 * @sql: a SQL statement
 * @error: (nullable): a #GError
 *
 * Prepare @sql and return a #ValentSqlStmt.
 *
 * Returns: (transfer full): a #ValentSqlStmt
 */
ValentSqlStmt *
valent_sql_db_prepare (ValentSqlDb  *db,
                         const char  *sql,
                         GError     **error)
{
  g_autoptr (ValentSqlStmt) stmt = NULL;
  sqlite3_stmt *sqlstmt;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);
  g_return_val_if_fail (sql != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!valent_sql_db_check (db, error))
    return FALSE;

  if (sqlite3_prepare_v2 (db->sqldb, sql, -1, &sqlstmt, 0) != SQLITE_OK)
    {
      g_set_error (error, G_FILE_ERROR,
                   G_FILE_ERROR_FAILED,
                   "Error preparing: %s",
                   sqlite3_errmsg (db->sqldb));
      return NULL;
    }

  stmt = g_atomic_rc_box_new0 (ValentSqlStmt);
  stmt->stmt = sqlstmt;
  stmt->n_columns = sqlite3_column_count (sqlstmt);
  stmt->n_params = sqlite3_bind_parameter_count (sqlstmt);

  return g_steal_pointer (&stmt);
}

/**
 * valent_sql_db_step:
 * @db: a #ValentSqlDb
 * @stmt: a #ValentSqlStmt
 * @error: (nullable): a #GError
 *
 * Execute an iteration of @stmt, returning a #ValentSqlStep. If %VALENT_SQL_STEP_ERR
 * is returned, @error will be set.
 *
 * Returns: a #ValentSqlStep enum
 */
ValentSqlStep
valent_sql_db_step (ValentSqlDb    *db,
                    ValentSqlStmt  *stmt,
                    GError        **error)
{
  gint rc;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);
  g_return_val_if_fail (stmt != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!valent_sql_db_check (db, error))
    return FALSE;

  /* Step through the results */
  if ((rc = sqlite3_step (stmt->stmt)) == SQLITE_ROW)
    {
      stmt->n_columns = sqlite3_column_count (stmt->stmt);
      return VALENT_SQL_STEP_ROW;
    }

  if (rc != SQLITE_DONE)
    {
      g_set_error (error, G_FILE_ERROR,
                   G_FILE_ERROR_FAILED,
                   "Error stepping: %s",
                   sqlite3_errmsg (db->sqldb));
      return VALENT_SQL_STEP_ERR;
    }

  return VALENT_SQL_STEP_END;
}

/**
 * valent_sql_db_stmt:
 * @db: a #ValentSqlDb
 * @stmt: a #ValentSqlStmt
 * @error: (nullable): a #GError
 *
 * Execute a prepared #ValentSqlDbStmt, stepping through all rows. Returns %TRUE
 * on success or %FALSE with @error set.
 *
 * Returns: %TRUE if successful.
 */
gboolean
valent_sql_db_stmt (ValentSqlDb     *db,
                    ValentSqlStmt  *stmt,
                    GError        **error)
{
  gint rc;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);
  g_return_val_if_fail (stmt != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!valent_sql_db_check (db, error))
    return FALSE;

  /* Step through the results */
  while ((rc = sqlite3_step (stmt->stmt)) == SQLITE_ROW)
    continue;

  if (rc != SQLITE_DONE)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   G_FILE_ERROR_FAILED,
                   "Error executing: %s",
                   sqlite3_errmsg (db->sqldb));
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_sql_db_foreach:
 * @db: a #ValentSqlDb
 * @stmt: a #ValentSqlStmt
 * @func: (nullable) (scope call): a #ValentSqlFunc
 * @user_data: (closure): user supplied data
 * @error: (nullable): a #GError
 *
 * Execute @func for each result row of @stmt. If @func returns %FALSE or if
 * there are no rows, this function will immediately return %TRUE.
 *
 * If this function returns %FALSE, @error will be set.
 *
 * Returns: %TRUE if successful
 */
gboolean
valent_sql_db_foreach (ValentSqlDb     *db,
                         ValentSqlStmt  *stmt,
                         ValentSqlFunc   func,
                         gpointer        user_data,
                         GError        **error)
{
  gint rc;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);
  g_return_val_if_fail (stmt != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!valent_sql_db_check (db, error))
    return FALSE;

  /* Step through the results */
  while ((rc = sqlite3_step(stmt->stmt)) == SQLITE_ROW)
    {
      /* If the user function returns %FALSE we're done */
      if (func && !func (stmt, user_data))
        return TRUE;
    }

  if (rc != SQLITE_DONE)
    {
      g_set_error (error, G_FILE_ERROR,
                   G_FILE_ERROR_FAILED,
                   "Error executing: %s",
                   sqlite3_errmsg(db->sqldb));
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_sql_db_select:
 * @db: a #ValentSqlDb
 * @sql: a `SELECT` query
 * @func: (nullable) (scope call): a #ValentSqlFunc
 * @user_data: (closure): user supplied data
 * @error: (nullable): a #GError
 *
 * A convenience function for `SELECT` statements that prepares a statement for
 * @sql and calls valent_sql_db_foreach().
 *
 * If this function returns %FALSE, @error will be set.
 *
 * Returns: %TRUE if successful
 */
gboolean
valent_sql_db_select (ValentSqlDb    *db,
                      const char     *sql,
                      ValentSqlFunc   func,
                      gpointer        user_data,
                      GError        **error)
{
  g_autoptr (ValentSqlStmt) stmt = NULL;

  g_return_val_if_fail (VALENT_IS_SQL_DB (db), FALSE);
  g_return_val_if_fail (sql != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* Prepare the statement */
  stmt = valent_sql_db_prepare (db, sql, error);

  if (stmt == NULL)
    return FALSE;

  return valent_sql_db_foreach (db, stmt, func, user_data, error);
}
