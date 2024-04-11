/* SPDX-License-Identifier: GPL-2.0-or-later
 * SPDX-FileCopyrightText: 2002-2012 Bastien Nocera <hadess@hadess.net>
 * SPDX-FileContributor: 2005 Christian Schaller
 *
 * Copyright © 2002-2012 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 * The Totem project hereby grant permission for non-gpl compatible GStreamer
 * plugins to be used and distributed together with GStreamer and Totem. This
 * permission is above and beyond the permissions granted by the GPL license
 * Totem is covered by.
 *
 * Monday 7th February 2005: Christian Schaller: Add exception clause.
 * See license_change file for details.
 *
 */

#include "config.h"

#include <math.h>
#include <glib/gi18n-lib.h>
#include <libintl.h>

#include "valent-ui-utils-private.h"


/* FIXME: Remove
 * See https://gitlab.freedesktop.org/gstreamer/gstreamer/issues/26 */
char *
valent_media_time_to_string (int64_t       msecs,
                             TotemTimeFlag flags)
{
  int64_t _time;
  int msec, sec, min, hour;

  if (msecs < 0) {
    /* translators: Unknown time */
    return g_strdup (_("--:--"));
  }

  /* When calculating the remaining time,
   * we want to make sure that:
   * current time + time remaining = total run time */
  msec = msecs % 1000;
  if (flags & TOTEM_TIME_FLAG_MSECS) {
    _time = msecs - msec;
    _time = _time / 1000;
  } else {
    double time_f;

    time_f = (double) msecs / 1000;
    if (flags & TOTEM_TIME_FLAG_REMAINING)
      time_f = ceil (time_f);
    else
      time_f = round (time_f);
    _time = (int64_t) time_f;
  }

  sec = _time % 60;
  _time = _time - sec;
  min = (_time % (60*60)) / 60;
  _time = _time - (min * 60);
  hour = _time / (60*60);

  if (hour > 0 || flags & TOTEM_TIME_FLAG_FORCE_HOUR) {
    if (!(flags & TOTEM_TIME_FLAG_REMAINING)) {
      if (!(flags & TOTEM_TIME_FLAG_MSECS)) {
        /* hour:minutes:seconds */
        /* Translators: This is a time format, like "9:05:02" for 9
         * hours, 5 minutes, and 2 seconds. You may change ":" to
         * the separator that your locale uses or use "%Id" instead
         * of "%d" if your locale uses localized digits.
         */
        return g_strdup_printf (C_("long time format", "%d:%02d:%02d"), hour, min, sec);
      } else {
        /* hour:minutes:seconds.msecs */
        /* Translators: This is a time format, like "9:05:02.050" for 9
         * hours, 5 minutes, 2 seconds and 50 milliseconds. You may change ":" to
         * the separator that your locale uses or use "%Id" instead
         * of "%d" if your locale uses localized digits.
         */
        return g_strdup_printf (C_("long time format", "%d:%02d:%02d.%03d"), hour, min, sec, msec);
      }
    } else {
      if (!(flags & TOTEM_TIME_FLAG_MSECS)) {
        /* -hour:minutes:seconds */
        /* Translators: This is a time format, like "-9:05:02" for 9
         * hours, 5 minutes, and 2 seconds playback remaining. You may
         * change ":" to the separator that your locale uses or use
         * "%Id" instead of "%d" if your locale uses localized digits.
         */
        return g_strdup_printf (C_("long time format", "-%d:%02d:%02d"), hour, min, sec);
      } else {
        /* -hour:minutes:seconds.msecs */
        /* Translators: This is a time format, like "-9:05:02.050" for 9
         * hours, 5 minutes, 2 seconds and 50 milliseconds playback remaining. You may
         * change ":" to the separator that your locale uses or use
         * "%Id" instead of "%d" if your locale uses localized digits.
         */
        return g_strdup_printf (C_("long time format", "-%d:%02d:%02d.%03d"), hour, min, sec, msec);
      }
    }
  }

  if (flags & TOTEM_TIME_FLAG_REMAINING) {
    if (!(flags & TOTEM_TIME_FLAG_MSECS)) {
      /* -minutes:seconds */
      /* Translators: This is a time format, like "-5:02" for 5
       * minutes and 2 seconds playback remaining. You may change
       * ":" to the separator that your locale uses or use "%Id"
       * instead of "%d" if your locale uses localized digits.
       */
      return g_strdup_printf (C_("short time format", "-%d:%02d"), min, sec);
    } else {
      /* -minutes:seconds.msec */
      /* Translators: This is a time format, like "-5:02.050" for 5
       * minutes 2 seconds and 50 milliseconds playback remaining. You may change
       * ":" to the separator that your locale uses or use "%Id"
       * instead of "%d" if your locale uses localized digits.
       */
      return g_strdup_printf (C_("short time format", "-%d:%02d.%03d"), min, sec, msec);
    }
  }

  if (flags & TOTEM_TIME_FLAG_MSECS) {
    /* minutes:seconds.msec */
    /* Translators: This is a time format, like "5:02" for 5
     * minutes 2 seconds and 50 milliseconds. You may change ":" to the
     * separator that your locale uses or use "%Id" instead of
     * "%d" if your locale uses localized digits.
     */
    return g_strdup_printf (C_("short time format", "%d:%02d.%03d"), min, sec, msec);
  }

  /* minutes:seconds */
  /* Translators: This is a time format, like "5:02" for 5
   * minutes and 2 seconds. You may change ":" to the
   * separator that your locale uses or use "%Id" instead of
   * "%d" if your locale uses localized digits.
   */
  return g_strdup_printf (C_("short time format", "%d:%02d"), min, sec);
}

