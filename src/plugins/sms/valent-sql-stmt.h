// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <sqlite3.h>

G_BEGIN_DECLS

/**
 * ValentSqlStep:
 * @VALENT_SQL_STEP_ROW: a result row
 * @VALENT_SQL_STEP_END: the end of the results
 * @VALENT_SQL_STEP_ERR: an error occurred
 *
 * Return values for valent_data_sql_step().
 */
typedef enum
{
  VALENT_SQL_STEP_ROW,
  VALENT_SQL_STEP_END,
  VALENT_SQL_STEP_ERR
} ValentSqlStep;


typedef struct _ValentSqlStmt ValentSqlStmt;

struct _ValentSqlStmt
{
  sqlite3_stmt *stmt;
  unsigned int  n_columns;
  unsigned int  n_params;
};

GType           valent_sql_stmt_get_type    (void) G_GNUC_CONST;


ValentSqlStmt * valent_sql_stmt_new         (void);

GBytes        * valent_sql_stmt_dup_bytes   (ValentSqlStmt *stmt,
                                             unsigned int   column);
const guint8  * valent_sql_stmt_get_data    (ValentSqlStmt *stmt,
                                             unsigned int   column,
                                             gsize         *size);
int             valent_sql_stmt_get_int     (ValentSqlStmt *stmt,
                                             unsigned int   column);
gint64          valent_sql_stmt_get_int64   (ValentSqlStmt *stmt,
                                             unsigned int   column);
const char    * valent_sql_stmt_get_string  (ValentSqlStmt *stmt,
                                             unsigned int   column);
GVariant      * valent_sql_stmt_get_variant (ValentSqlStmt *stmt,
                                             unsigned int   column);

void            valent_sql_stmt_set_data    (ValentSqlStmt *stmt,
                                             unsigned int   param,
                                             const guint8  *data,
                                             gsize          size);
void            valent_sql_stmt_set_bytes   (ValentSqlStmt *stmt,
                                             unsigned int   param,
                                             GBytes        *bytes);
void            valent_sql_stmt_set_int     (ValentSqlStmt *stmt,
                                             unsigned int   param,
                                             int            value);
void            valent_sql_stmt_set_int64   (ValentSqlStmt *stmt,
                                             unsigned int   param,
                                             gint64         value);
void            valent_sql_stmt_set_string  (ValentSqlStmt *stmt,
                                             unsigned int   param,
                                             const char    *value);

void            valent_sql_stmt_reset       (ValentSqlStmt *stmt);

ValentSqlStmt * valent_sql_stmt_ref         (ValentSqlStmt *stmt);
void            valent_sql_stmt_unref       (ValentSqlStmt *stmt);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (ValentSqlStmt, valent_sql_stmt_unref)

G_END_DECLS
