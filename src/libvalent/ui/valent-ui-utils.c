// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-utils"

#include "config.h"

#include <gdk-pixbuf/gdk-pixdata.h>
#include <glib/gi18n.h>

#include "valent-ui-utils.h"


/**
 * SECTION:valentuiutils
 * @short_description: Utilities for working with UI
 * @title: UI Utilities
 * @stability: Unstable
 * @include: libvalent-ui.h
 *
 * Helper functions and utilities for working with UI elements.
 */


/**
 * valent_ui_pixbuf_from_base64:
 * @base64: BASE64 encoded data
 * @error: (nullable): a #GError
 *
 * Create a new #GdkPixbuf for @base64.
 *
 * Returns: (transfer full) (nullable): a new #GdkPixbuf
 */
GdkPixbuf *
valent_ui_pixbuf_from_base64 (const char  *base64,
                              GError     **error)
{
  g_autoptr (GdkPixbufLoader) loader = NULL;
  g_autoptr (GdkPixbuf) pixbuf = NULL;
  g_autofree guchar *data = NULL;
  gsize dlen;

  data = g_base64_decode (base64, &dlen);

  loader = gdk_pixbuf_loader_new();

  if (gdk_pixbuf_loader_write (loader, data, dlen, error) &&
      gdk_pixbuf_loader_close (loader, error))
    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

  return g_steal_pointer (&pixbuf);
}


/**
 * valent_ui_timestamp:
 * @timestamp: a UNIX epoch timestamp (ms)
 *
 * Create a user friendly date-time string for @timestamp, in a relative format.
 *
 * Examples:
 *     - "Just now"
 *     - "15 minutes"
 *     - "11:45 PM"
 *     - "Yesterday · 11:45 PM"
 *     - "Tuesday"
 *     - "February 29"
 *
 * Returns: (transfer full): a new string
 */
char *
valent_ui_timestamp (gint64 timestamp)
{
  g_autoptr (GDateTime) dt = NULL;
  g_autoptr (GDateTime) now = NULL;
  GTimeSpan diff;

  dt = g_date_time_new_from_unix_local (timestamp / 1000);
  now = g_date_time_new_now_local ();
  diff = g_date_time_difference (now, dt);

  /* TRANSLATORS: Less than a minute ago */
  if (diff < G_TIME_SPAN_MINUTE)
      return g_strdup (_("Just now"));

  /* TRANSLATORS: Time duration in minutes (eg. 15 minutes) */
  if (diff < G_TIME_SPAN_HOUR)
    {
      unsigned int n_minutes;

      n_minutes = (diff / G_TIME_SPAN_MINUTE);
      return g_strdup_printf (ngettext("%d minute", "%d minutes", n_minutes),
                              n_minutes);
    }

  /* TRANSLATORS: Yesterday, but less than 24 hours (eg. Yesterday · 11:45 PM) */
  if (diff < G_TIME_SPAN_DAY)
    {
      g_autofree char *time_str = NULL;
      int today, day;

      today = g_date_time_get_day_of_month(now);
      day = g_date_time_get_day_of_month(dt);
      time_str = g_date_time_format(dt, "%l:%M %p");

      if (today == day)
        return g_steal_pointer (&time_str);
      else
        return g_strdup_printf (_("Yesterday · %s"), time_str);
    }

  /* Less than a week ago (eg. Tuesday) */
  if (diff < G_TIME_SPAN_DAY * 7)
    return g_date_time_format(dt, "%A");

  /* More than a week ago (eg. February 29) */
  return g_date_time_format(dt, "%B %e");
}

/**
 * valent_ui_timestamp_short:
 * @timestamp: a UNIX epoch timestamp (ms)
 *
 * Create a user friendly date-time string for @timestamp, in a relative format.
 * This is like valent_ui_timestamp() but abbreviated.
 *
 * Examples:
 *     - "Just now"
 *     - "15 mins"
 *     - "11:45 PM"
 *     - "Tue"
 *     - "Feb 29"
 *
 * Returns: (transfer full): a new string
 */
char *
valent_ui_timestamp_short (gint64 timestamp)
{
  g_autoptr (GDateTime) dt = NULL;
  g_autoptr (GDateTime) now = NULL;
  GTimeSpan diff;

  dt = g_date_time_new_from_unix_local (timestamp / 1000);
  now = g_date_time_new_now_local ();
  diff = g_date_time_difference (now, dt);

  // TRANSLATORS: Less than a minute ago
  if (diff < G_TIME_SPAN_MINUTE)
      return g_strdup (_("Just now"));

  // TRANSLATORS: Time duration in minutes, abbreviated (eg. 15 mins)
  if (diff < G_TIME_SPAN_HOUR)
    {
      unsigned int n_minutes;

      n_minutes = (diff / G_TIME_SPAN_MINUTE);
      return g_strdup_printf (ngettext ("%d min", "%d mins", n_minutes),
                              n_minutes);
    }


  /* Less than a day ago (eg. 11:45 PM) */
  if (diff < G_TIME_SPAN_DAY)
    return g_date_time_format (dt, "%l:%M %p");

  /* Less than a week ago (eg. Tue) */
  if (diff < G_TIME_SPAN_DAY * 7)
    return g_date_time_format (dt, "%a");

  /* More than a week ago (eg. Feb 29) */
  return g_date_time_format (dt, "%b %e");
}

