// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-target"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-share-target.h"
#include "valent-share-target-chooser.h"


struct _ValentShareTarget
{
  ValentApplicationPlugin  parent_instance;

  GPtrArray               *windows;
};

G_DEFINE_FINAL_TYPE (ValentShareTarget, valent_share_target, VALENT_TYPE_APPLICATION_PLUGIN)


static void
on_destroy (GtkWindow         *window,
            ValentShareTarget *self)
{
  unsigned int index;

  g_assert (GTK_IS_WINDOW (window));
  g_assert (VALENT_IS_SHARE_TARGET (self));

  /* The signal was emitted because we're disposing or being disabled */
  if (self->windows == NULL)
    return;

  g_ptr_array_find (self->windows, window, &index);

  if (g_ptr_array_find (self->windows, window, &index))
    g_ptr_array_steal_index (self->windows, index);
}

/*
 * ValentApplicationPlugin
 */
static void
valent_share_target_enable (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_SHARE_TARGET (plugin));
}

static void
valent_share_target_disable (ValentApplicationPlugin *plugin)
{
  ValentShareTarget *self = VALENT_SHARE_TARGET (plugin);

  g_assert (VALENT_IS_SHARE_TARGET (plugin));

  g_ptr_array_remove_range (self->windows, 0, self->windows->len);
}

static gboolean
valent_share_target_open (ValentApplicationPlugin  *plugin,
                          GFile                   **files,
                          int                       n_files,
                          const char               *hint)
{
  ValentShareTarget *self = VALENT_SHARE_TARGET (plugin);
  ValentDeviceManager *manager = NULL;
  g_autoptr (GListStore) list = NULL;
  GtkWindow *window = NULL;

  g_assert (VALENT_IS_SHARE_TARGET (plugin));
  g_assert (files != NULL);
  g_assert (n_files > 0);
  g_assert (hint != NULL);

  list = g_list_store_new (G_TYPE_FILE);

  for (int i = 0; i < n_files; i++)
    g_list_store_append (list, files[i]);

  manager = valent_application_plugin_get_device_manager (plugin);

  window = g_object_new (VALENT_TYPE_SHARE_TARGET_CHOOSER,
                         "device-manager", manager,
                         "files",          list,
                         NULL);

  g_signal_connect_object (G_OBJECT (window),
                           "destroy",
                           G_CALLBACK (on_destroy),
                           self, 0);
  g_ptr_array_add (self->windows, window);

  gtk_window_present (window);

  return TRUE;
}

/*
 * GObject
 */
static void
valent_share_target_dispose (GObject *object)
{
  ValentShareTarget *self = VALENT_SHARE_TARGET (object);

  g_clear_pointer (&self->windows, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_share_target_parent_class)->dispose (object);
}

static void
valent_share_target_class_init (ValentShareTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  object_class->dispose = valent_share_target_dispose;

  plugin_class->enable = valent_share_target_enable;
  plugin_class->disable = valent_share_target_disable;
  plugin_class->open = valent_share_target_open;
}

static void
valent_share_target_init (ValentShareTarget *self)
{
  self->windows = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_window_destroy);
}

