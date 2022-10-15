// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-object-utils"

#include "config.h"

#include "valent-object.h"
#include "valent-macros.h"


/**
 * valent_object_list_free:
 * @list: (type GLib.List) (element-type GObject.Object) (nullable): a #GList
 *
 * A convenience function for freeing a #GSList of #GObject.
 *
 * Since: 1.0
 */
void
valent_object_list_free (gpointer list)
{
  g_list_free_full (list, g_object_unref);
}

/**
 * valent_object_slist_free:
 * @slist: (type GLib.SList) (element-type GObject.Object) (nullable): a #GSList
 *
 * A convenience function for freeing a #GSList of #GObject.
 *
 * Since: 1.0
 */
void
valent_object_slist_free (gpointer slist)
{
  g_slist_free_full (slist, g_object_unref);
}

