// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-input-remote.h"
#include "valent-media-window.h"
#include "valent-messages-window.h"
#include "valent-share-dialog.h"
#include "valent-ui-utils-private.h"
#include "valent-window.h"

#include "valent-gnome-application.h"


struct _ValentGNOMEApplication
{
  ValentApplicationPlugin  parent_instance;

  GtkWindow               *main_window;
  GtkWindow               *input_remote;
  GtkWindow               *media_window;
  GtkWindow               *messages_window;
  GPtrArray               *windows;
};

G_DEFINE_FINAL_TYPE (ValentGNOMEApplication, valent_gnome_application, VALENT_TYPE_APPLICATION_PLUGIN)


/*
 * GActions
 */
static void
main_window_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (user_data);

  g_assert (VALENT_IS_GNOME_APPLICATION (self));

  if (self->main_window == NULL)
    {
      ValentDeviceManager *devices = valent_device_manager_get_default ();
      GApplication *application = valent_extension_get_object (VALENT_EXTENSION (self));

      self->main_window = g_object_new (VALENT_TYPE_WINDOW,
                                        "default-width",  600,
                                        "default-height", 480,
                                        "device-manager", devices,
                                        NULL);
      g_object_add_weak_pointer (G_OBJECT (self->main_window),
                                 (gpointer)&self->main_window);

      gtk_widget_insert_action_group (GTK_WIDGET (self->main_window),
                                      "app",
                                      G_ACTION_GROUP (application));
    }

  gtk_window_present (self->main_window);
  gtk_widget_activate_action_variant (GTK_WIDGET (self->main_window),
                                      "win.page",
                                      parameter);
}

static void
input_remote_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (user_data);

  g_assert (VALENT_IS_GNOME_APPLICATION (self));

  if (self->input_remote == NULL)
    {
      self->input_remote = g_object_new (VALENT_TYPE_INPUT_REMOTE,
                                         "adapters", valent_input_get_default (),
                                         NULL);
      g_object_add_weak_pointer (G_OBJECT (self->input_remote),
                                 (gpointer)&self->input_remote);
    }

  gtk_window_present (self->input_remote);
}

static void
media_window_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (user_data);

  g_assert (VALENT_IS_GNOME_APPLICATION (self));

  if (self->media_window == NULL)
    {
      self->media_window = g_object_new (VALENT_TYPE_MEDIA_WINDOW,
                                         "players", valent_media_get_default (),
                                         NULL);
      g_object_add_weak_pointer (G_OBJECT (self->media_window),
                                 (gpointer)&self->media_window);
    }

  gtk_window_present (self->media_window);
}

static void
messages_window_action (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (user_data);

  g_assert (VALENT_IS_GNOME_APPLICATION (self));

  if (self->messages_window == NULL)
    {
      self->messages_window = g_object_new (VALENT_TYPE_MESSAGES_WINDOW,
                                            "messages", valent_messages_get_default (),
                                            NULL);
      g_object_add_weak_pointer (G_OBJECT (self->messages_window),
                                 (gpointer)&self->messages_window);
    }

  gtk_window_present (self->messages_window);
}

static void
on_destroy (GtkWindow       *window,
            ValentGNOMEApplication *self)
{
  unsigned int index;

  g_assert (GTK_IS_WINDOW (window));
  g_assert (VALENT_IS_GNOME_APPLICATION (self));

  /* The signal was emitted because we're disposing or being disabled */
  if (self->windows == NULL)
    return;

  if (g_ptr_array_find (self->windows, window, &index))
    g_ptr_array_steal_index (self->windows, index);
}

static void
valent_share_target_present (ValentGNOMEApplication *self,
                             GListModel      *files)
{
  GtkWindow *window = NULL;

  g_assert (VALENT_IS_GNOME_APPLICATION (self));
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

static inline void
share_dialog_action (GSimpleAction *action,
                     GVariant      *parameters,
                     gpointer       user_data)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (user_data);

  g_assert (VALENT_IS_GNOME_APPLICATION (self));

  valent_share_target_present (self, NULL);
}

static const GActionEntry app_actions[] = {
  { "input-remote",    input_remote_action,    NULL, NULL, NULL },
  { "media-window",    media_window_action,    NULL, NULL, NULL },
  { "messages-window", messages_window_action, NULL, NULL, NULL },
  { "share-dialog",    share_dialog_action,    NULL, NULL, NULL },
  { "window",          main_window_action,     "s",  NULL, NULL },
};


/*
 * ValentApplicationPlugin
 */
static gboolean
valent_gnome_application_activate (ValentApplicationPlugin *plugin)
{
  GApplication *application = NULL;

  g_assert (VALENT_IS_GNOME_APPLICATION (plugin));

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));
  g_action_group_activate_action (G_ACTION_GROUP (application),
                                  "window",
                                  g_variant_new_string ("main"));

  return TRUE;
}

static gboolean
valent_gnome_application_open (ValentApplicationPlugin  *plugin,
                        GFile                   **files,
                        int                       n_files,
                        const char               *hint)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (plugin);
  g_autoptr (GListStore) files_list = NULL;

  g_assert (VALENT_IS_GNOME_APPLICATION (plugin));
  g_assert (files != NULL);
  g_assert (n_files > 0);
  g_assert (hint != NULL);

  files_list = g_list_store_new (G_TYPE_FILE);
  g_list_store_splice (files_list, 0, 0, (gpointer *)files, n_files);
  valent_share_target_present (self, G_LIST_MODEL (files_list));

  return TRUE;
}

static void
valent_gnome_application_shutdown (ValentApplicationPlugin *plugin)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (plugin);
  GApplication *application = NULL;

  g_assert (VALENT_IS_GNOME_APPLICATION (plugin));

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));

  for (size_t i = 0; i < G_N_ELEMENTS (app_actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), app_actions[i].name);

  g_clear_pointer (&self->media_window, gtk_window_destroy);
  g_clear_pointer (&self->main_window, gtk_window_destroy);
}

static void
valent_gnome_application_startup (ValentApplicationPlugin *plugin)
{
  GApplication *application = NULL;

  g_assert (VALENT_IS_GNOME_APPLICATION (plugin));

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   app_actions,
                                   G_N_ELEMENTS (app_actions),
                                   plugin);
}

/*
 * ValentObject
 */
static void
valent_gnome_application_destroy (ValentObject *object)
{
  ValentGNOMEApplication *self = VALENT_GNOME_APPLICATION (object);
  GApplication *application = NULL;

  application = valent_extension_get_object (VALENT_EXTENSION (self));

  for (size_t i = 0; i < G_N_ELEMENTS (app_actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), app_actions[i].name);

  g_clear_pointer (&self->windows, g_ptr_array_unref);

  VALENT_OBJECT_CLASS (valent_gnome_application_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_gnome_application_class_init (ValentGNOMEApplicationClass *klass)
{
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  vobject_class->destroy = valent_gnome_application_destroy;

  plugin_class->activate = valent_gnome_application_activate;
  plugin_class->open = valent_gnome_application_open;
  plugin_class->shutdown = valent_gnome_application_shutdown;
  plugin_class->startup = valent_gnome_application_startup;

  valent_ui_init ();
}

static void
valent_gnome_application_init (ValentGNOMEApplication *self)
{
  self->windows = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_window_destroy);
}

