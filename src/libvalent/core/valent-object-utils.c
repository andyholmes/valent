// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-object-utils"

#include "config.h"

#include "valent-object.h"
#include "valent-macros.h"


typedef struct
{
  GRecMutex   mutex;
  GWeakRef    object;
  GParamSpec *pspec;
  char       *property_name;
} NotifyEmission;


static gboolean
valent_object_notify_main (gpointer data)
{
  NotifyEmission *emission = data;
  g_autoptr (GObject) object = NULL;

  g_assert (emission != NULL);

  g_rec_mutex_lock (&emission->mutex);
  if ((object = g_weak_ref_get (&emission->object)) != NULL)
    {
      if (emission->pspec != NULL)
        g_object_notify_by_pspec (object, emission->pspec);
      else
        g_object_notify (object, emission->property_name);
    }

  g_weak_ref_clear (&emission->object);
  g_clear_pointer (&emission->property_name, g_free);
  g_clear_pointer (&emission->pspec, g_param_spec_unref);
  g_rec_mutex_unlock (&emission->mutex);
  g_rec_mutex_clear (&emission->mutex);
  g_clear_pointer (&emission, g_free);

  return G_SOURCE_REMOVE;
}


/**
 * valent_object_notify:
 * @object: (type GObject.Object): a #GObject
 * @property_name: a property name
 *
 * Emit [signal@GObject.Object::notify] on @object, on the main thread.
 *
 * Like [method@GObject.Object.notify] if the caller is in the main thread,
 * otherwise the invocation is deferred to the main thread.
 *
 * Since: 1.0
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
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->object, object);
  emission->property_name = g_strdup (property_name);
  g_rec_mutex_unlock (&emission->mutex);

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
 * Emit [signal@GObject.Object::notify] on @object, on the main thread.
 *
 * Like [method@GObject.Object.notify_by_pspec] if the caller is in the main
 * thread, otherwise the invocation is deferred to the main thread.
 *
 * Since: 1.0
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
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->object, object);
  emission->pspec = g_param_spec_ref (pspec);
  g_rec_mutex_unlock (&emission->mutex);

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

