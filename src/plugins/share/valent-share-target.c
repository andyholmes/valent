// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-target"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-share-dialog.h"
#include "valent-share-target.h"


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

  if (g_ptr_array_find (self->windows, window, &index))
    g_ptr_array_steal_index (self->windows, index);
}

static void
valent_share_target_present (ValentShareTarget *self,
                             GListModel        *files)
{
  GtkWindow *window = NULL;

  g_assert (VALENT_IS_SHARE_TARGET (self));
  g_assert (files == NULL || G_IS_LIST_MODEL (files));

  window = g_object_new (VALENT_TYPE_SHARE_DIALOG,
                         "files", files,
                         NULL);
  g_signal_connect_object (G_OBJECT (window),
                           "destroy",
                           G_CALLBACK (on_destroy),
                           self,
                           G_CONNECT_DEFAULT);
  g_ptr_array_add (self->windows, window);

  gtk_window_present (window);
}

/*
 * GActions
 */
static inline void
share_dialog_action (GSimpleAction *action,
                     GVariant      *parameters,
                     gpointer       user_data)
{
  ValentShareTarget *self = VALENT_SHARE_TARGET (user_data);

  g_assert (VALENT_IS_SHARE_TARGET (self));

  valent_share_target_present (self, NULL);
}

static const GActionEntry app_actions[] = {
  { "share-dialog", share_dialog_action, NULL, NULL, NULL },
};

/*
 * ValentApplicationPlugin
 */
static gboolean
valent_share_target_open (ValentApplicationPlugin  *plugin,
                          GFile                   **files,
                          int                       n_files,
                          const char               *hint)
{
  ValentShareTarget *self = VALENT_SHARE_TARGET (plugin);
  g_autoptr (GListStore) files_list = NULL;

  g_assert (VALENT_IS_SHARE_TARGET (plugin));
  g_assert (files != NULL);
  g_assert (n_files > 0);
  g_assert (hint != NULL);

  files_list = g_list_store_new (G_TYPE_FILE);
  g_list_store_splice (files_list, 0, 0, (gpointer *)files, n_files);
  valent_share_target_present (self, G_LIST_MODEL (files_list));

  return TRUE;
}

/*
 * ValentObject
 */
static void
valent_share_target_destroy (ValentObject *object)
{
  ValentShareTarget *self = VALENT_SHARE_TARGET (object);
  GApplication *application = NULL;

  application = valent_extension_get_object (VALENT_EXTENSION (self));

  for (size_t i = 0; i < G_N_ELEMENTS (app_actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), app_actions[i].name);

  g_clear_pointer (&self->windows, g_ptr_array_unref);

  VALENT_OBJECT_CLASS (valent_share_target_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_share_target_constructed (GObject *object)
{
  ValentShareTarget *self = VALENT_SHARE_TARGET (object);
  GApplication *application = NULL;

  G_OBJECT_CLASS (valent_share_target_parent_class)->constructed (object);

  application = valent_extension_get_object (VALENT_EXTENSION (self));
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   app_actions,
                                   G_N_ELEMENTS (app_actions),
                                   self);
}

static void
valent_share_target_class_init (ValentShareTargetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  object_class->constructed = valent_share_target_constructed;

  vobject_class->destroy = valent_share_target_destroy;

  plugin_class->open = valent_share_target_open;
}

static void
valent_share_target_init (ValentShareTarget *self)
{
  self->windows = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_window_destroy);
}

