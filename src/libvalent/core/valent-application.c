// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-application"

#include "config.h"

#include <gio/gio.h>

#include "valent-application.h"
#include "valent-application-plugin.h"
#include "valent-component-private.h"
#include "valent-debug.h"
#include "valent-global.h"


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
  GApplication   parent_instance;

  GHashTable    *plugins;
  ValentContext *plugins_context;
};

G_DEFINE_FINAL_TYPE (ValentApplication, valent_application, G_TYPE_APPLICATION)


/*
 * PeasEngine
 */
static inline void
valent_application_enable_plugin (ValentApplication *self,
                                  ValentPlugin      *plugin)
{
  g_assert (VALENT_IS_APPLICATION (self));

  plugin->extension = peas_engine_create_extension (valent_get_plugin_engine (),
                                                    plugin->info,
                                                    VALENT_TYPE_APPLICATION_PLUGIN,
                                                    "object", self,
                                                    NULL);
  g_return_if_fail (G_IS_OBJECT (plugin->extension));
}

static inline void
valent_application_disable_plugin (ValentApplication *self,
                                   ValentPlugin      *plugin)
{
  g_assert (VALENT_IS_APPLICATION (self));

  g_clear_object (&plugin->extension);
}

static void
on_plugin_enabled_changed (ValentPlugin *plugin)
{
  g_assert (plugin != NULL);
  g_assert (VALENT_IS_APPLICATION (plugin->parent));

  if (valent_plugin_get_enabled (plugin))
    valent_application_enable_plugin (plugin->parent, plugin);
  else
    valent_application_disable_plugin (plugin->parent, plugin);
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

  plugin = valent_plugin_new (self, self->plugins_context, info,
                              G_CALLBACK (on_plugin_enabled_changed));
  g_hash_table_insert (self->plugins, info, plugin);

  if (valent_plugin_get_enabled (plugin))
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
      if (plugin->extension == NULL)
        continue;

      if (valent_application_plugin_activate (VALENT_APPLICATION_PLUGIN (plugin->extension)))
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
      if (plugin->extension == NULL)
        continue;

      valent_application_plugin_startup (VALENT_APPLICATION_PLUGIN (plugin->extension));
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
      if (plugin->extension == NULL)
        continue;

      valent_application_plugin_shutdown (VALENT_APPLICATION_PLUGIN (plugin->extension));
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
      if (plugin->extension == NULL)
        continue;

      if (!valent_application_plugin_dbus_register (VALENT_APPLICATION_PLUGIN (plugin->extension),
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
      if (plugin->extension == NULL)
        continue;

      valent_application_plugin_dbus_unregister (VALENT_APPLICATION_PLUGIN (plugin->extension),
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
  const GList *plugins = NULL;

  self->plugins = g_hash_table_new_full (NULL, NULL, NULL, valent_plugin_free);
  self->plugins_context = valent_context_new (NULL, "application", NULL);

  engine = valent_get_plugin_engine ();
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

  G_OBJECT_CLASS (valent_application_parent_class)->constructed (object);
}

static void
valent_application_dispose (GObject *object)
{
  ValentApplication *self = VALENT_APPLICATION (object);
  PeasEngine *engine = NULL;

  engine = valent_get_plugin_engine ();
  g_signal_handlers_disconnect_by_data (engine, self);

  g_hash_table_remove_all (self->plugins);
  g_clear_pointer (&self->plugins, g_hash_table_unref);
  g_clear_object (&self->plugins_context);

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

