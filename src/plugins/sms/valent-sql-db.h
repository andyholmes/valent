// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

#include "valent-sql-stmt.h"

G_BEGIN_DECLS

/**
 * ValentSqlFunc:
 * @stmt: A #ValentSqlStmt
 * @user_data: (closure): user supplied data
 *
 * This function is passed to valent_sql_db_foreach() and will be called for
 * each row returned by @stmt.
 */
typedef gboolean  (*ValentSqlFunc) (ValentSqlStmt *stmt,
                                    gpointer       user_data);


#define VALENT_TYPE_SQL_DB (valent_sql_db_get_type())

G_DECLARE_FINAL_TYPE (ValentSqlDb, valent_sql_db, VALENT, SQL_DB, GObject)

ValentSqlDb   * valent_sql_db_new     (const char      *path);
void            valent_sql_db_lock    (ValentSqlDb     *db);
void            valent_sql_db_unlock  (ValentSqlDb     *db);

gboolean        valent_sql_db_open    (ValentSqlDb     *db,
                                       GError         **error);
void            valent_sql_db_close   (ValentSqlDb     *db);
gboolean        valent_sql_db_begin   (ValentSqlDb     *db);
gboolean        valent_sql_db_commit  (ValentSqlDb     *db);
gboolean        valent_sql_db_exec    (ValentSqlDb     *db,
                                       const char      *sql,
                                       GError         **error);
ValentSqlStmt * valent_sql_db_prepare (ValentSqlDb     *db,
                                       const char      *sql,
                                       GError         **error);
ValentSqlStep   valent_sql_db_step    (ValentSqlDb     *db,
                                       ValentSqlStmt   *stmt,
                                       GError         **error);
gboolean        valent_sql_db_stmt    (ValentSqlDb     *db,
                                       ValentSqlStmt   *stmt,
                                       GError         **error);
gboolean        valent_sql_db_foreach (ValentSqlDb     *db,
                                       ValentSqlStmt   *stmt,
                                       ValentSqlFunc    func,
                                       gpointer         user_data,
                                       GError         **error);
gboolean        valent_sql_db_select  (ValentSqlDb     *db,
                                       const char      *sql,
                                       ValentSqlFunc    func,
                                       gpointer         user_data,
                                       GError         **error);

G_END_DECLS

