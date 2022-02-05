// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

#include "valent-sms-store.h"

G_BEGIN_DECLS

void   valent_sms_store_get_thread_items  (ValentSmsStore      *store,
                                           gint64               thread_id,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data);


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
 * additional data is stored as serialized #GVariant data in metadata.
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

/**
 * REMOVE_MESSAGE_SQL:
 *
 * Remove the message for `id`.
 */
#define REMOVE_MESSAGE_SQL                     \
"DELETE FROM message"                          \
"  WHERE id=?;"                                \

/**
 * REMOVE_THREAD_SQL:
 *
 * Remove the messages for `thread_id`.
 */
#define REMOVE_THREAD_SQL                      \
"DELETE FROM message"                          \
"  WHERE thread_id=?;"                         \

/**
 * FIND_MESSAGES_SQL:
 *
 * Find the latest message in each thread matching the query.
 */
#define FIND_MESSAGES_SQL                      \
"SELECT * FROM message"                        \
"  WHERE (thread_id, date) IN ("               \
"    SELECT thread_id, MAX(date) FROM message" \
"      WHERE text LIKE ? GROUP BY thread_id"   \
"  );"

/**
 * GET_MESSAGE_SQL:
 *
 * Get the message for`id`.
 */
#define GET_MESSAGE_SQL                        \
"SELECT * FROM message"                        \
"  WHERE id=?;"                                \

/**
 * GET_THREAD_SQL:
 *
 * Get the messages for `thread_id`, ascending by date.
 */
#define GET_THREAD_SQL                         \
"SELECT * FROM message"                        \
"  WHERE thread_id=? ORDER BY date ASC;"       \

/**
 * GET_THREAD_DATE_SQL:
 *
 * Get the date of the most recent message for `thread_id`.
 */
#define GET_THREAD_DATE_SQL                    \
"SELECT date FROM message"                     \
"  WHERE thread_id=? ORDER BY date DESC"       \
"  LIMIT 1;"

/**
 * GET_THREAD_ITEMS_SQL:
 *
 * Get the `date` and `id` for each message in @thread_id
 */
#define GET_THREAD_ITEMS_SQL             \
"SELECT date, id, sender FROM message"   \
"  WHERE thread_id=? ORDER BY date ASC;"

/**
 * GET_SUMMARY_SQL:
 *
 * Get the most recent message for each thread.
 */
#define GET_SUMMARY_SQL                        \
"SELECT * FROM message"                        \
"  WHERE (thread_id, date) IN ("               \
"    SELECT thread_id, MAX(date) FROM message" \
"    GROUP BY thread_id"                       \
"  )"                                          \
"  ORDER BY date DESC;"

G_END_DECLS

