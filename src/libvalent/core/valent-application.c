// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-application"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "valent-application-plugin.h"
#include "valent-data-source.h"
#include "valent-debug.h"
#include "valent-global.h"
#include "valent-plugin.h"

#include "valent-application.h"

/**
 * ValentApplication:
 *
 * The primary application class of Valent.
 *
 * `ValentApplication` is the primary application class for Valent.
 *
 * Since: 1.0
 */

struct _ValentApplication
{
  GApplication   parent_instance;

  GHashTable    *plugins;
};

G_DEFINE_FINAL_TYPE (ValentApplication, valent_application, G_TYPE_APPLICATION)


/*
 * PeasEngine
 */
static void
on_plugin_enabled_changed (ValentPlugin      *plugin,
                           GParamSpec        *pspec,
                           ValentApplication *self)
{
  g_assert (VALENT_IS_PLUGIN (plugin));
  g_assert (VALENT_IS_APPLICATION (self));

  if (valent_plugin_get_enabled (plugin))
    {
      g_autoptr (ValentExtension) extension = NULL;

      extension = valent_plugin_create_extension (plugin);
      valent_plugin_set_extension (plugin, extension);
    }
  else
    {
      valent_plugin_set_extension (plugin, NULL);
    }
}

static void
on_load_plugin (PeasEngine        *engine,
                PeasPluginInfo    *info,
                ValentApplication *self)
{
  ValentPlugin *plugin = NULL;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_APPLICATION (self));

  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_APPLICATION_PLUGIN))
    return;

  VALENT_NOTE ("%s: %s",
               g_type_name (VALENT_TYPE_APPLICATION_PLUGIN),
               peas_plugin_info_get_module_name (info));

  plugin = valent_plugin_new (valent_data_source_get_local_default (),
                              info,
                              VALENT_TYPE_APPLICATION_PLUGIN,
                              "application");
  g_signal_connect_object (plugin,
                           "notify::enabled",
                           G_CALLBACK (on_plugin_enabled_changed),
                           self,
                           G_CONNECT_DEFAULT);
  g_hash_table_insert (self->plugins, info, plugin);
  on_plugin_enabled_changed (plugin, NULL, self);
}

static void
on_unload_plugin (PeasEngine        *engine,
                  PeasPluginInfo    *info,
                  ValentApplication *self)
{
  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_APPLICATION (self));

  if (!peas_engine_provides_extension (engine, info, VALENT_TYPE_APPLICATION_PLUGIN))
    return;

  g_hash_table_remove (self->plugins, info);
}


/*
 * GActions
 */
static void
quit_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  GApplication *application = G_APPLICATION (user_data);

  g_assert (G_IS_APPLICATION (application));

  g_application_quit (application);
}

static const GActionEntry app_actions[] = {
  { "quit", quit_action, NULL, NULL, NULL },
};


/*
 * GApplication
 */
static void
valent_application_activate (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GHashTableIter iter;
  ValentPlugin *plugin;

  g_assert (VALENT_IS_APPLICATION (self));

  g_hash_table_iter_init (&iter, self->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      ValentApplicationPlugin *extension = valent_plugin_get_extension (plugin);

      if (extension != NULL && valent_application_plugin_activate (extension))
        return;
    }

  g_debug ("%s(): unhandled activation", G_STRFUNC);
}

static void
valent_application_open (GApplication  *application,
                         GFile        **files,
                         int            n_files,
                         const char    *hint)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GHashTableIter iter;
  ValentPlugin *plugin;

  g_assert (VALENT_IS_APPLICATION (self));

  g_hash_table_iter_init (&iter, self->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      ValentApplicationPlugin *extension = valent_plugin_get_extension (plugin);

      if (extension == NULL)
        continue;

      if (valent_application_plugin_open (extension, files, n_files, hint))
        return;
    }

  g_warning ("%s(): %i unhandled files", G_STRFUNC, n_files);
}

static void
valent_application_startup (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GHashTableIter iter;
  ValentPlugin *plugin;

  g_assert (VALENT_IS_APPLICATION (application));

  /* Chain-up first */
  G_APPLICATION_CLASS (valent_application_parent_class)->startup (application);
  g_application_hold (application);

  g_action_map_add_action_entries (G_ACTION_MAP (application),
                                   app_actions,
                                   G_N_ELEMENTS (app_actions),
                                   application);

  g_hash_table_iter_init (&iter, self->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      ValentApplicationPlugin *extension = valent_plugin_get_extension (plugin);

      if (extension != NULL)
        valent_application_plugin_startup (extension);
    }
}

static void
valent_application_shutdown (GApplication *application)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GHashTableIter iter;
  ValentPlugin *plugin;

  g_hash_table_iter_init (&iter, self->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      ValentApplicationPlugin *extension = valent_plugin_get_extension (plugin);

      if (extension != NULL)
        valent_application_plugin_shutdown (extension);
    }

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
  GHashTableIter iter;
  ValentPlugin *plugin;

  /* Chain-up first */
  if (!klass->dbus_register (application, connection, object_path, error))
    return FALSE;

  g_hash_table_iter_init (&iter, self->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      ValentApplicationPlugin *extension = valent_plugin_get_extension (plugin);

      if (extension == NULL)
        continue;

      if (!valent_application_plugin_dbus_register (extension,
                                                    connection,
                                                    object_path,
                                                    error))
        return FALSE;
    }

  return TRUE;
}

static void
valent_application_dbus_unregister (GApplication    *application,
                                    GDBusConnection *connection,
                                    const char      *object_path)
{
  ValentApplication *self = VALENT_APPLICATION (application);
  GApplicationClass *klass = G_APPLICATION_CLASS (valent_application_parent_class);
  GHashTableIter iter;
  ValentPlugin *plugin;

  g_hash_table_iter_init (&iter, self->plugins);
  while (g_hash_table_iter_next (&iter, NULL, (void **)&plugin))
    {
      ValentApplicationPlugin *extension = valent_plugin_get_extension (plugin);

      if (extension == NULL)
        continue;

      valent_application_plugin_dbus_unregister (extension,
                                                 connection,
                                                 object_path);
    }

  /* Chain-up last */
  klass->dbus_unregister (application, connection, object_path);
}


/*
 * GObject
 */
static void
valent_application_constructed (GObject *object)
{
  ValentApplication *self = VALENT_APPLICATION (object);
  PeasEngine *engine = NULL;
  unsigned int n_plugins = 0;

  G_OBJECT_CLASS (valent_application_parent_class)->constructed (object);

  engine = valent_get_plugin_engine ();
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

  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (engine));
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) info = NULL;

      info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      if (peas_plugin_info_is_loaded (info))
        on_load_plugin (engine, info, self);
    }
}

static void
valent_application_dispose (GObject *object)
{
  ValentApplication *self = VALENT_APPLICATION (object);

  g_signal_handlers_disconnect_by_data (valent_get_plugin_engine (), self);
  g_hash_table_remove_all (self->plugins);
  g_clear_pointer (&self->plugins, g_hash_table_unref);

  G_OBJECT_CLASS (valent_application_parent_class)->dispose (object);
}

static void
valent_application_class_init (ValentApplicationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

  object_class->constructed = valent_application_constructed;
  object_class->dispose = valent_application_dispose;

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
  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}

GApplication *
_valent_application_new (void)
{
  return g_object_new (VALENT_TYPE_APPLICATION,
                       "application-id",     APPLICATION_ID,
                       "resource-base-path", "/ca/andyholmes/Valent",
                       "flags",              G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

