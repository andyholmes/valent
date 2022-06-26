// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-application"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

#include "valent-application.h"
#include "valent-window.h"


/**
 * ValentApplication:
 *
 * The primary application class of Valent.
 *
 * #ValentApplication is the primary application class for Valent.
 *
 * Since: 1.0
 */

struct _ValentApplication
{
  GtkApplication       parent_instance;

  GSettings           *settings;
  ValentDeviceManager *manager;
  GHashTable          *plugins;
  GtkWindow           *window;
};

G_DEFINE_TYPE (ValentApplication, valent_application, GTK_TYPE_APPLICATION)


/*
 * PeasEngine
 */
typedef struct
{
  GApplication   *application;
  PeasPluginInfo *info;
  PeasExtension  *extension;
  GSettings      *settings;
} ApplicationPlugin;

static void
application_plugin_free (gpointer data)
{
  ApplicationPlugin *plugin = data;

  /* We guarantee calling valent_application_plugin_disable() */
  if (plugin->extension != NULL)
    {
      valent_application_plugin_disable (VALENT_APPLICATION_PLUGIN (plugin->extension));
      g_clear_object (&plugin->extension);
    }

  g_clear_object (&plugin->settings);
  g_clear_pointer (&plugin, g_free);
}

static inline void
valent_application_enable_plugin (ValentApplication *self,
                                  ApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_APPLICATION (self));

  plugin->extension = peas_engine_create_extension (valent_get_engine (),
                                                    plugin->info,
                                                    VALENT_TYPE_APPLICATION_PLUGIN,
                                                    "application",    self,
                                                    "device-manager", self->manager,
                                                    NULL);
  g_return_if_fail (PEAS_IS_EXTENSION (plugin->extension));

  valent_application_plugin_enable (VALENT_APPLICATION_PLUGIN (plugin->extension));
}

static inline void
valent_application_disable_plugin (ValentApplication *self,
                                   ApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_APPLICATION (self));

  /* We guarantee calling valent_application_plugin_disable() */
  if (plugin->extension != NULL)
    {
      valent_application_plugin_disable (VALENT_APPLICATION_PLUGIN (plugin->extension));
      g_clear_object (&plugin->extension);
    }
}

static void
on_enabled_changed (GSettings         *settings,
                    const char        *key,
                    ApplicationPlugin *plugin)
{
  ValentApplication *self = VALENT_APPLICATION (plugin->application);

  g_assert (G_IS_SETTINGS (settings));
  g_assert (VALENT_IS_APPLICATION (self));

  g_debug ("%s: %s", G_STRFUNC, peas_plugin_info_get_module_name (plugin->info));

  if (g_settings_get_boolean (settings, key))
    valent_application_enable_plugin (self, plugin);
  else
    valent_application_disable_plugin (self, plugin);
}

static void
on_load_plugin (PeasEngine        *engine,
                PeasPluginInfo    *info,
                ValentApplication *self)
{
  ApplicationPlugin *plugin = NULL;
  const char *module;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_APPLICATION (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_APPLICATION_PLUGIN))
    return;

  VALENT_NOTE ("%s: %s",
               g_type_name (VALENT_TYPE_APPLICATION_PLUGIN),
               peas_plugin_info_get_module_name (info));

  module = peas_plugin_info_get_module_name (info);

  plugin = g_new0 (ApplicationPlugin, 1);
  plugin->application = G_APPLICATION (self);
  plugin->info = info;
  plugin->settings = valent_component_create_settings ("application", module);
  g_hash_table_insert (self->plugins, info, plugin);

  /* The PeasExtension is created and destroyed based on the enabled state */
  g_signal_connect (plugin->settings,
                    "changed::enabled",
                    G_CALLBACK (on_enabled_changed),
                    plugin);

  if (g_settings_get_boolean (plugin->settings, "enabled"))
    valent_application_enable_plugin (self, plugin);
}

static void
on_unload_plugin (PeasEngine        *engine,
                  PeasPluginInfo    *info,
                  ValentApplication *self)
{
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_APPLICATION (self));

  /* We're only interested in one GType */
  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_APPLICATION_PLUGIN))
    return;

  g_hash_table_remove (self->plugins, info);
}


/*
 * ValentApplication
 */
static void
valent_application_load_plugins (ValentApplication *self)
{
  PeasEngine *engine = NULL;
  const GList *plugins = NULL;

  g_assert (VALENT_IS_APPLICATION (self));

  self->plugins = g_hash_table_new_full (NULL,
                                         NULL,
                                         NULL,
                                         application_plugin_free);

  engine = valent_get_engine ();
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (peas_plugin_info_is_loaded (iter->data))
        on_load_plugin (engine, iter->data, self);
    }

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (on_load_plugin),
                           self,
                           G_CONNECT_AFTER);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_plugin),
                           self,
                           0);
}

static void
valent_application_unload_plugins (ValentApplication *self)
{
  PeasEngine *engine = NULL;

  g_assert (VALENT_IS_APPLICATION (self));

  engine = valent_get_engine ();
  g_signal_handlers_disconnect_by_data (engine, self);

  g_hash_table_remove_all (self->plugins);
  g_clear_pointer (&self->plugins, g_hash_table_unref);
}

static void
valent_application_present_window (ValentApplication *self,
                                   const char        *startup_id)
{
  g_assert (VALENT_IS_APPLICATION (self));

  if (self->window == NULL)
    {
      self->window = g_object_new (VALENT_TYPE_WINDOW,
                                   "application",    self,
                                   "default-width",  600,
                                   "default-height", 480,
                                   "device-manager", self->manager,
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (self->window),
                                 (gpointer) &self->window);
    }

  if (startup_id != NULL)
    gtk_window_set_startup_id (self->window, startup_id);

  gtk_window_present_with_time (self->window, GDK_CURRENT_TIME);
}

/*
 * GActions
 */
static void
device_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentApplication *self = VALENT_APPLICATION (user_data);
  ValentDevice *device;
  const char *device_id;
  const char *name;
  g_autoptr (GVariantIter) targetv = NULL;
  g_autoptr (GVariant) target = NULL;

  g_assert (VALENT_IS_APPLICATION (self));

  /* Device ID, action name, array holding optional action parameter */
  g_variant_get (parameter, "(&s&sav)", &device_id, &name, &targetv);
  g_variant_iter_next (targetv, "v", &target);

  /* Forward the activation */
  device = valent_device_manager_get_device (self->manager, device_id);

  if (device != NULL)
    g_action_group_activate_action (G_ACTION_GROUP (device), name, target);
}

static void
preferences_action (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  ValentApplication *self = VALENT_APPLICATION (user_data);

  g_assert (VALENT_IS_APPLICATION (self));

  valent_application_present_window (self, NULL);
}

static void
quit_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  GApplication *application = G_APPLICATION (user_data);

  g_assert (G_IS_APPLICATION (application));

  g_application_quit (application);
}

static void
refresh_action (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  ValentApplication *self = VALENT_APPLICATION (user_data);

  g_assert (VALENT_IS_APPLICATION (self));

  valent_device_manager_identify (self->manager, NULL);
}

static const GActionEntry actions[] = {
  { "device",      device_action,      "(ssav)", NULL, NULL },
  { "preferences", preferences_action, NULL,     NULL, NULL },
  { "quit",        quit_action,        NULL,     NULL, NULL },
  { "refresh",     refresh_action,     NULL,     NULL, NULL }
};


/*
 * GApplication
 */
static void
valent_application_activate (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GHashTableIter iter;
  ApplicationPlugin *plugin;

  g_assert (VALENT_IS_APPLICATION (self));

  /* Run the plugin handlers */
  g_hash_table_iter_init (&iter, self->plugins);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      if (plugin->extension == NULL)
        continue;

      if (valent_application_plugin_activate (VALENT_APPLICATION_PLUGIN (plugin->extension)))
        return;
    }

  /* If no plugin takes ownership of the activation, present the main window */
  valent_application_present_window (self, NULL);
}

static void
valent_application_open (GApplication  *application,
                         GFile        **files,
                         int            n_files,
                         const char    *hint)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GHashTableIter iter;
  ApplicationPlugin *plugin;

  g_assert (VALENT_IS_APPLICATION (self));

  /* Run the plugin handlers */
  g_hash_table_iter_init (&iter, self->plugins);

  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      if (plugin->extension == NULL)
        continue;

      if (valent_application_plugin_open (VALENT_APPLICATION_PLUGIN (plugin->extension),
                                          files,
                                          n_files,
                                          hint))
        return;
    }

  /* If no plugin takes ownership of the files, print a warning. */
  g_warning ("%s(): %i unhandled files", G_STRFUNC, n_files);
}

static void
valent_application_startup (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  g_autofree char *name = NULL;

  g_assert (VALENT_IS_APPLICATION (application));

  /* Chain-up first */
  G_APPLICATION_CLASS (valent_application_parent_class)->startup (application);

  g_application_hold (application);
  adw_init ();

  /* Service Actions */
  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   application);

  /* Device Name */
  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_settings_bind (self->settings, "name",
                   self->manager,  "name",
                   G_SETTINGS_BIND_DEFAULT);
  name = g_settings_get_string (self->settings, "name");
  valent_device_manager_set_name (self->manager, name);

  /* Load plugins and start the device manager */
  valent_application_load_plugins (self);
  valent_device_manager_start (self->manager);
}

static void
valent_application_shutdown (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);

  g_assert (VALENT_IS_APPLICATION (application));

  g_clear_pointer (&self->window, gtk_window_destroy);
  valent_device_manager_stop (self->manager);
  valent_application_unload_plugins (self);
  g_clear_object (&self->settings);

  G_APPLICATION_CLASS (valent_application_parent_class)->shutdown (application);
}

static gboolean
valent_application_dbus_register (GApplication     *application,
                                  GDBusConnection  *connection,
                                  const char       *object_path,
                                  GError          **error)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GApplicationClass *klass = G_APPLICATION_CLASS (valent_application_parent_class);

  g_assert (VALENT_IS_APPLICATION (self));

  /* Chain-up first */
  if (!klass->dbus_register (application, connection, object_path, error))
    return FALSE;

  self->manager = valent_device_manager_new_sync (NULL, NULL, error);

  if (self->manager == NULL)
    return FALSE;

  valent_device_manager_export (self->manager, connection, object_path);

  return TRUE;
}

static void
valent_application_dbus_unregister (GApplication    *application,
                                    GDBusConnection *connection,
                                    const char      *object_path)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GApplicationClass *klass = G_APPLICATION_CLASS (valent_application_parent_class);

  g_assert (VALENT_IS_APPLICATION (self));

  if (self->manager != NULL)
    {
      valent_device_manager_unexport (self->manager);
      g_clear_object (&self->manager);
    }

  /* Chain-up last */
  klass->dbus_unregister (application, connection, object_path);
}


/*
 * GObject
 */
static void
valent_application_class_init (ValentApplicationClass *klass)
{
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  application_class->activate = valent_application_activate;
  application_class->startup = valent_application_startup;
  application_class->shutdown = valent_application_shutdown;
  application_class->open = valent_application_open;
  application_class->dbus_register = valent_application_dbus_register;
  application_class->dbus_unregister = valent_application_dbus_unregister;
}

static void
valent_application_init (ValentApplication *self)
{
}

ValentApplication *
_valent_application_new (void)
{
  return g_object_new (VALENT_TYPE_APPLICATION,
                       "application-id",     APPLICATION_ID,
                       "resource-base-path", "/ca/andyholmes/Valent",
                       "flags",              G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

