// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/**
 * ValentTimeFlag:
 * @TOTEM_TIME_FLAG_NONE: Default behaviour
 * @TOTEM_TIME_FLAG_REMAINING: Time remaining
 * @TOTEM_TIME_FLAG_FORCE_HOUR: Always include the hourly duration
 * @TOTEM_TIME_FLAG_MSECS: Always include the millisecond duration
 *
 * Time duration flags.
 *
 * Since: 1.0
 */
typedef enum {
  TOTEM_TIME_FLAG_NONE,
  TOTEM_TIME_FLAG_REMAINING  = (1 << 0),
  TOTEM_TIME_FLAG_FORCE_HOUR = (1 << 2),
  TOTEM_TIME_FLAG_MSECS      = (1 << 3),
} TotemTimeFlag;

char * valent_media_time_to_string (gint64        msecs,
                                    TotemTimeFlag flags);

G_END_DECLS
