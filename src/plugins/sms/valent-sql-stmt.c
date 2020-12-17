// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sql-stmt"

#include "config.h"

#include <gio/gio.h>
#include <sqlite3.h>

#include "valent-sql-stmt.h"


/**
 * SECTION:valent-sql-stmt
 * @short_description: A boxed type for database queries
 * @name: ValentSqlStmt
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * The #ValentSqlStmt type is a simple boxed type for SQL queries.
 */

G_DEFINE_BOXED_TYPE (ValentSqlStmt, valent_sql_stmt, valent_sql_stmt_ref, valent_sql_stmt_unref)


static inline void
valent_sql_stmt_free (gpointer data)
{
  ValentSqlStmt *stmt = data;

  g_clear_pointer (&stmt->stmt, sqlite3_finalize);
}

/**
 * valent_sql_stmt_new:
 *
 * Create a new, unprepared #ValentSqlStmt. Free with valent_sql_stmt_unref().
 *
 * Returns: (transfer full): a #ValentSqlStmt.
 */
ValentSqlStmt *
valent_sql_stmt_new (void)
{
  return g_atomic_rc_box_new0 (ValentSqlStmt);
}

/**
 * valent_sql_stmt_ref:
 * @stmt: a #ValentSqlStmt
 *
 * Increase the reference count of @stmt.
 *
 * Returns: (transfer full): a #ValentSqlStmt.
 */
ValentSqlStmt *
valent_sql_stmt_ref (ValentSqlStmt *stmt)
{
  g_return_val_if_fail (stmt != NULL, NULL);

  return g_atomic_rc_box_acquire (stmt);
}

/**
 * valent_sql_stmt_unref:
 * @stmt: a #ValentSqlStmt
 *
 * Decreases the reference count of @stmt. When its reference count drops to 0,
 * the statement is finalized (i.e. its memory is freed).
 */
void
valent_sql_stmt_unref (ValentSqlStmt *stmt)
{
  g_return_if_fail (stmt != NULL);

  g_atomic_rc_box_release_full (stmt, valent_sql_stmt_free);
}

/**
 * valent_sql_stmt_reset:
 * @stmt: a #ValentSqlStmt
 *
 * Reset @stmt, as with `sqlite3_reset()`.
 */
void
valent_sql_stmt_reset (ValentSqlStmt *stmt)
{
  g_return_if_fail (stmt != NULL);

  sqlite3_reset (stmt->stmt);
}

/**
 * valent_sql_stmt_get_data:
 * @stmt: a #ValentSqlStmt
 * @column: column number
 * @size: (out) (optional): location to return size of byte data
 *
 * Get the value of @column as binary data.
 *
 * Returns: (transfer none): a pointer to the bytearray
 */
const guint8 *
valent_sql_stmt_get_data (ValentSqlStmt *stmt,
                          unsigned int   column,
                          gsize         *size)
{
  g_return_val_if_fail (stmt != NULL, 0);
  g_return_val_if_fail (column < stmt->n_columns, NULL);

  if (size)
    *size = sqlite3_column_bytes (stmt->stmt, column);

  return sqlite3_column_blob (stmt->stmt, column);
}

/**
 * valent_sql_stmt_dup_bytes:
 * @stmt: a #ValentSqlStmt
 * @column: column number
 *
 * Get the value of @column as a new #GBytes.
 *
 * Returns: (transfer full): a #GBytes.
 */
GBytes *
valent_sql_stmt_dup_bytes (ValentSqlStmt *stmt,
                           unsigned int   column)
{
  const guint8 *data;
  gsize size;

  g_return_val_if_fail (stmt != NULL, 0);
  g_return_val_if_fail (column < stmt->n_columns, NULL);

  size = sqlite3_column_bytes (stmt->stmt, column);
  data = sqlite3_column_blob (stmt->stmt, column);

  return g_bytes_new (data, size);
}

/**
 * valent_sql_stmt_get_int:
 * @stmt: a #ValentSqlStmt
 * @column: column number
 *
 * Get the integer value of @column.
 *
 * Returns: a 32-bit integer
 */
int
valent_sql_stmt_get_int (ValentSqlStmt *stmt,
                         unsigned int   column)
{
  g_return_val_if_fail (stmt != NULL, 0);
  g_return_val_if_fail (column < stmt->n_columns, 0);

  return sqlite3_column_int(stmt->stmt, column);
}

/**
 * valent_sql_stmt_get_int64:
 * @stmt: a #ValentSqlStmt
 * @column: column number
 *
 * Get the integer value of @column.
 *
 * Returns: a 64-bit integer
 */
gint64
valent_sql_stmt_get_int64 (ValentSqlStmt *stmt,
                           unsigned int   column)
{
  g_return_val_if_fail (stmt != NULL, 0);
  g_return_val_if_fail (column < stmt->n_columns, 0);

  return sqlite3_column_int64(stmt->stmt, column);
}

/**
 * valent_sql_stmt_get_string:
 * @stmt: a #ValentSqlStmt
 * @column: column number
 *
 * Get the text value of @column.
 *
 * Returns: (transfer none): a %NULL-terminated string
 */
const char *
valent_sql_stmt_get_string (ValentSqlStmt *stmt,
                            unsigned int   column)
{
  g_return_val_if_fail (stmt != NULL, NULL);
  g_return_val_if_fail (column < stmt->n_columns, NULL);

  return (const char *)sqlite3_column_text(stmt->stmt, column);
}

/**
 * valent_sql_stmt_get_variant:
 * @stmt: a #ValentSqlStmt
 * @column: column number
 *
 * Get the value of @column as a non-floating #GVariant. This is a convenience
 * for calling valent_sql_stmt_get_string() and g_variant_parse().
 *
 * If the type of @column is not `TEXT` or parsing the contents fails, this
 * function will return %NULL.
 *
 * Returns: (transfer full) (nullable): a #GVariant
 */
GVariant *
valent_sql_stmt_get_variant (ValentSqlStmt *stmt,
                             unsigned int   column)
{
  const char *data = NULL;

  g_return_val_if_fail (stmt != NULL, NULL);
  g_return_val_if_fail (column < stmt->n_columns, NULL);

  data = (const char *)sqlite3_column_text (stmt->stmt, column);

  return (data) ? g_variant_parse (NULL, data, NULL, NULL, NULL) : NULL;
}

/**
 * valent_sql_stmt_set_data:
 * @stmt: a #ValentSqlStmt
 * @param: column param
 * @data: binary data
 * @size: size
 *
 * Set @param to @value.
 */
void
valent_sql_stmt_set_data (ValentSqlStmt *stmt,
                          unsigned int   param,
                          const guint8  *data,
                          gsize          size)
{
  g_return_if_fail (stmt != NULL);
  //g_return_if_fail (param > 0 && param < stmt->n_params);

  sqlite3_bind_blob (stmt->stmt, param, data, size, NULL);
}

/**
 * valent_sql_stmt_set_bytes:
 * @stmt: a #ValentSqlStmt
 * @param: column param
 * @bytes: a #GBytes
 *
 * Set @param to @bytes.
 */
void
valent_sql_stmt_set_bytes (ValentSqlStmt *stmt,
                           unsigned int   param,
                           GBytes        *bytes)
{
  gconstpointer data;
  gsize size;

  g_return_if_fail (stmt != NULL);
  //g_return_if_fail (param > 0 && param < stmt->n_params);

  data = g_bytes_get_data (bytes, &size);
  sqlite3_bind_blob (stmt->stmt, param, data, size, NULL);
}

/**
 * valent_sql_stmt_set_int:
 * @stmt: a #ValentSqlStmt
 * @param: column param
 * @value: integer
 *
 * Set @param to @value.
 */
void
valent_sql_stmt_set_int (ValentSqlStmt *stmt,
                         unsigned int   param,
                         int            value)
{
  g_return_if_fail (stmt != NULL);
  //g_return_if_fail (param > 0 && param < stmt->n_params);

  sqlite3_bind_int64 (stmt->stmt, param, value);
}

/**
 * valent_sql_stmt_set_int64:
 * @stmt: a #ValentSqlStmt
 * @param: column param
 * @value: 64-bit integer
 *
 * Set @param to @value.
 */
void
valent_sql_stmt_set_int64 (ValentSqlStmt *stmt,
                           unsigned int   param,
                           gint64         value)
{
  g_return_if_fail (stmt != NULL);
  //g_return_if_fail (param > 0 && param < stmt->n_params);

  sqlite3_bind_int64 (stmt->stmt, param, value);
}

/**
 * valent_sql_stmt_set_string:
 * @stmt: a #ValentSqlStmt
 * @param: column param
 * @value: %NULL-terminated string
 *
 * Set @param to @value.
 */
void
valent_sql_stmt_set_string (ValentSqlStmt *stmt,
                            unsigned int   param,
                            const char    *value)
{
  g_return_if_fail (stmt != NULL);
  //g_return_if_fail (param < stmt->n_params);

  sqlite3_bind_text (stmt->stmt, param, value, -1, NULL);
}

