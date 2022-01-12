// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-object-utils"

#include "config.h"

#include "valent-object.h"
#include "valent-macros.h"


typedef struct
{
  GObject    *object;
  GParamSpec *pspec;
  char       *property_name;
} NotifyEmission;


static gboolean
valent_object_notify_main (gpointer data)
{
  NotifyEmission *emission = data;

  g_assert (emission != NULL);
  g_assert (G_IS_OBJECT (emission->object));
  g_assert (emission->property_name != NULL || emission->pspec != NULL);

  if G_LIKELY (emission->pspec)
    g_object_notify_by_pspec (emission->object, emission->pspec);
  else
    g_object_notify (emission->object, emission->property_name);

  g_clear_object (&emission->object);
  g_clear_pointer (&emission->property_name, g_free);
  g_clear_pointer (&emission->pspec, g_param_spec_unref);
  g_free (emission);

  return G_SOURCE_REMOVE;
}


/**
 * valent_object_notify:
 * @object: (type GObject.Object): a #GObject
 * @property_name: a property name
 *
 * Like g_object_notify() if the caller is in the main-thread. Otherwise, the
 * request is deferred to the main thread.
 */
void
valent_object_notify (gpointer    object,
                      const char *property_name)
{
  NotifyEmission *emission = NULL;

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property_name != NULL);

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_object_notify (object, property_name);
      return;
    }

  emission = g_new0 (NotifyEmission, 1);
  emission->object = g_object_ref (object);
  emission->property_name = g_strdup (property_name);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   valent_object_notify_main,
                   g_steal_pointer (&emission),
                   NULL);
}

/**
 * valent_object_notify_by_pspec:
 * @object: (type GObject.Object): a #GObject
 * @pspec: a #GParamSpec
 *
 * Like g_object_notify_by_pspec() if the caller is in the main-thread.
 * Otherwise, the request is deferred to the main thread.
 */
void
valent_object_notify_by_pspec (gpointer    object,
                               GParamSpec *pspec)
{
  NotifyEmission *emission = NULL;

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_object_notify_by_pspec (object, pspec);
      return;
    }

  emission = g_new0 (NotifyEmission, 1);
  emission->object = g_object_ref (object);
  emission->pspec = g_param_spec_ref (pspec);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   valent_object_notify_main,
                   g_steal_pointer (&emission),
                   NULL);
}

/**
 * valent_object_list_free:
 * @list: (type GLib.List) (element-type GObject.Object) (nullable): a #GList
 *
 * A convenience function for freeing a #GSList of #GObject.
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
 */
void
valent_object_slist_free (gpointer slist)
{
  g_slist_free_full (slist, g_object_unref);
}

