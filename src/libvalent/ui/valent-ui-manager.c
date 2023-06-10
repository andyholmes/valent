// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-input.h>
#include <libvalent-media.h>

#include "valent-input-remote.h"
#include "valent-media-remote.h"
#include "valent-ui-manager.h"
#include "valent-ui-utils.h"
#include "valent-window.h"


struct _ValentUIManager
{
  ValentApplicationPlugin  parent_instance;

  GtkWindow               *main_window;
  GtkWindow               *input_remote;
  GtkWindow               *media_remote;
};

G_DEFINE_FINAL_TYPE (ValentUIManager, valent_ui_manager, VALENT_TYPE_APPLICATION_PLUGIN)


/*
 * GActions
 */
static void
main_window_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ValentUIManager *self = VALENT_UI_MANAGER (user_data);

  g_assert (VALENT_IS_UI_MANAGER (self));

  if (self->main_window == NULL)
    {
      ValentDeviceManager *devices = valent_device_manager_get_default ();

      self->main_window = g_object_new (VALENT_TYPE_WINDOW,
                                        "default-width",  600,
                                        "default-height", 480,
                                        "device-manager", devices,
                                        NULL);
      g_object_add_weak_pointer (G_OBJECT (self->main_window),
                                 (gpointer)&self->main_window);
    }

  gtk_window_present_with_time (self->main_window, GDK_CURRENT_TIME);
  gtk_widget_activate_action_variant (GTK_WIDGET (self->main_window),
                                      "win.page",
                                      parameter);
}

static void
input_remote_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ValentUIManager *self = VALENT_UI_MANAGER (user_data);

  g_assert (VALENT_IS_UI_MANAGER (self));

  if (self->input_remote == NULL)
    {
      self->input_remote = g_object_new (VALENT_TYPE_INPUT_REMOTE,
                                         "adapters", valent_input_get_default (),
                                         NULL);
      g_object_add_weak_pointer (G_OBJECT (self->input_remote),
                                 (gpointer)&self->input_remote);
    }

  gtk_window_present_with_time (self->input_remote, GDK_CURRENT_TIME);
}

static void
media_remote_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ValentUIManager *self = VALENT_UI_MANAGER (user_data);

  g_assert (VALENT_IS_UI_MANAGER (self));

  if (self->media_remote == NULL)
    {
      self->media_remote = g_object_new (VALENT_TYPE_MEDIA_REMOTE,
                                   "players", valent_media_get_default (),
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (self->media_remote),
                                 (gpointer)&self->media_remote);
    }

  gtk_window_present_with_time (self->media_remote, GDK_CURRENT_TIME);
}

static const GActionEntry app_actions[] = {
  { "input-remote", input_remote_action, NULL, NULL, NULL },
  { "media-remote", media_remote_action, NULL, NULL, NULL },
  { "window",       main_window_action,  "s",  NULL, NULL },
};


/*
 * ValentApplicationPlugin
 */
static gboolean
valent_ui_manager_activate (ValentApplicationPlugin *plugin)
{
  GApplication *application = NULL;

  g_assert (VALENT_IS_UI_MANAGER (plugin));

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));
  g_action_group_activate_action (G_ACTION_GROUP (application),
                                  "window",
                                  g_variant_new_string ("main"));

  return TRUE;
}

static void
valent_ui_manager_shutdown (ValentApplicationPlugin *plugin)
{
  ValentUIManager *self = VALENT_UI_MANAGER (plugin);
  GApplication *application = NULL;

  g_assert (VALENT_IS_UI_MANAGER (plugin));

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));

  for (unsigned int i = 0; i < G_N_ELEMENTS (app_actions); i++)
    g_action_map_remove_action (G_ACTION_MAP (application), app_actions[i].name);

  g_clear_pointer (&self->media_remote, gtk_window_destroy);
  g_clear_pointer (&self->main_window, gtk_window_destroy);
}

static void
valent_ui_manager_startup (ValentApplicationPlugin *plugin)
{
  GApplication *application = NULL;

  g_assert (VALENT_IS_UI_MANAGER (plugin));

  valent_ui_init ();

  application = valent_extension_get_object (VALENT_EXTENSION (plugin));
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   app_actions,
                                   G_N_ELEMENTS (app_actions),
                                   plugin);
}


/*
 * GObject
 */
static void
valent_ui_manager_class_init (ValentUIManagerClass *klass)
{
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  plugin_class->activate = valent_ui_manager_activate;
  plugin_class->shutdown = valent_ui_manager_shutdown;
  plugin_class->startup = valent_ui_manager_startup;
}

static void
valent_ui_manager_init (ValentUIManager *self)
{
}

