// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * MESSAGE_TABLE_SQL:
 *
 * @box: (type Valent.MessageBox):
 * @date: (type gint64): a UNIX epoch timestamp (ms)
 * @id: (type gint64): a message ID, unique to at least @thread_id
 * @metadata: (type GLib.Variant): additional serialized #GVariant data
 * @read: (type gboolean): the viewed status
 * @sender: (type utf8): the sender address
 * @text: (type utf8): the message content
 * @thread_id: (type gint64): a group ID
 *
 * The SQL query used to create the `message` table, which holds records of
 * abstract messages. The most commonly searched properties are fields, while
 * additional data is stored in serialized #GVariant data in metadata.
 *
 * In general, messages are organized in groups by @thread_id and sorted by
 * @date in ascending order. Each database entry is meant to map perfectly to
 * #ValentMessage, such that the column IDs match the property IDs and the
 * column values are equivalent or safe to cast.
 *
 * Additional data is found in the @metadata #GVariant dictionary.
 */
#define MESSAGE_TABLE_SQL              \
"CREATE TABLE IF NOT EXISTS message (" \
"  box       INTEGER NOT NULL,"        \
"  date      INTEGER NOT NULL,"        \
"  id        INTEGER NOT NULL,"        \
"  metadata  TEXT    NOT NULL,"        \
"  read      INTEGER NOT NULL,"        \
"  sender    TEXT,"                    \
"  text      TEXT    NOT NULL,"        \
"  thread_id INTEGER NOT NULL,"        \
"  UNIQUE(thread_id, id)"              \
");"

/**
 * PARTICIPANTS_TABLE_SQL:
 * @thread_id: (type gint64): a group ID
 * @address: (type utf8): recipient address
 * @metadata: (type GLib.Variant): additional serialized #GVariant data
 *
 * The SQL query used to create the `participant` table which, in contrast to
 * the `message` table, stores all the message recipients for each thread. This
 * uncomplicates collecting contacts and reply addresses for group conversations
 * and other things.
 */
#define PARTICIPANT_TABLE_SQL                                               \
"CREATE TABLE IF NOT EXISTS participant ("                                  \
"  address   TEXT    NOT NULL,"                                             \
"  metadata  INTEGER NOT NULL,"                                             \
"  thread_id INTEGER NOT NULL,"                                             \
"  FOREIGN KEY(thread_id) REFERENCES message(thread_id) ON DELETE CASCADE," \
"  UNIQUE(thread_id, address)"                                              \
");"

/**
 * GET_ITEM_SQL:
 * GET_ITEMS_SQL:
 * GET_N_ITEMS_SQL:
 *
 * These are the three SQL queries used in implementing #GListModel
 */
#define GET_ITEM_SQL                     \
"SELECT * FROM message"                  \
"  WHERE thread_id=? ORDER BY date ASC"  \
"  LIMIT 1 OFFSET ?;"

#define GET_N_ITEMS_SQL                  \
"SELECT COUNT(*) FROM message"           \
"  WHERE thread_id=?;"

#define LIST_ITEMS_SQL                   \
"SELECT id, date FROM message"           \
"  WHERE thread_id=? ORDER BY date ASC;"

/**
 * ADD_MESSAGE_SQL:
 *
 * Insert or update a message.
 */
#define ADD_MESSAGE_SQL                                                \
"INSERT INTO message(box,date,id,metadata,read,sender,text,thread_id)" \
"  VALUES (?, ?, ?, ?, ?, ?, ?, ?)"                                    \
"  ON CONFLICT(thread_id,id) DO UPDATE SET"                            \
"    box=excluded.box,"                                                \
"    date=excluded.date,"                                              \
"    metadata=excluded.metadata,"                                      \
"    read=excluded.read,"                                              \
"    sender=excluded.sender;"

#define ADD_PARTICIPANT_SQL                                            \
"INSERT INTO participant"                                              \
"  (thread_id, address) VALUES (?, ?);"                                \

/**
 * REMOVE_MESSAGE_SQL:
 *
 * Remove the message referred to by `thread_id` and `id`.
 */
#define REMOVE_MESSAGE_SQL                     \
"DELETE FROM message"                          \
"  WHERE thread_id=? AND id=?;"                \

/**
 * GET_MESSAGE_SQL:
 *
 * Get the row for `thread_id` and `id`.
 */
#define GET_MESSAGE_SQL                        \
"SELECT * FROM message"                        \
"  WHERE thread_id=? AND id=?;"                \

/**
 * GET_THREAD_SQL:
 *
 * Get the messages for `thread_id`, in order by ascending date.
 */
#define GET_THREAD_SQL                         \
"SELECT * FROM message"                        \
"  WHERE thread_id=? ORDER BY date ASC;"       \

/**
 * GET_THREAD_DATE_SQL:
 *
 * Get the messages for `thread_id`, in order by ascending date.
 */
#define GET_THREAD_DATE_SQL                    \
"SELECT date FROM message"                     \
"  WHERE thread_id=? ORDER BY date DESC"       \
"  LIMIT 1;"

/**
 * FIND_SQL:
 *
 * Find the latest message in each thread matching the query.
 */
#define FIND_SQL                               \
"SELECT * FROM message"                        \
"  WHERE (thread_id, date) IN ("               \
"    SELECT thread_id, MAX(date) FROM message" \
"    WHERE text LIKE ? GROUP BY thread_id"     \
"  );"

/**
 * GET_SUMMARY_SQL:
 *
 * Find the latest message in each thread.
 */
#define GET_SUMMARY_SQL                        \
"SELECT * FROM message"                        \
"  WHERE (thread_id, date) IN ("               \
"    SELECT thread_id, MAX(date) FROM message" \
"    GROUP BY thread_id"                       \
"  )"                                          \
"  ORDER BY date DESC;"

G_END_DECLS

