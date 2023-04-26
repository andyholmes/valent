// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>

#include "valent-mock-application-plugin.h"


struct _ValentMockApplicationPlugin
{
  ValentApplicationPlugin  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockApplicationPlugin, valent_mock_application_plugin, VALENT_TYPE_APPLICATION_PLUGIN)


/*
 * ValentApplicationPlugin
 */
static void
valent_mock_application_plugin_enable (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));
}

static void
valent_mock_application_plugin_disable (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));
}

static gboolean
valent_mock_application_plugin_activate (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));

  return TRUE;
}

static int
valent_mock_application_plugin_command_line (ValentApplicationPlugin *plugin,
                                             GApplicationCommandLine *command_line)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));
  g_assert (G_IS_APPLICATION_COMMAND_LINE (command_line));

  return 0;
}

static gboolean
valent_mock_application_plugin_open (ValentApplicationPlugin  *plugin,
                                     GFile                   **files,
                                     int                       n_files,
                                     const char               *hint)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));

  return TRUE;
}

static gboolean
valent_mock_application_plugin_dbus_register (ValentApplicationPlugin  *plugin,
                                              GDBusConnection          *connection,
                                              const char               *object_path,
                                              GError                  **error)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));
  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (g_variant_is_object_path (object_path));
  g_assert (error == NULL || *error == NULL);

  return TRUE;
}

static void
valent_mock_application_plugin_dbus_unregister (ValentApplicationPlugin *plugin,
                                                GDBusConnection         *connection,
                                                const char              *object_path)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));
  g_assert (G_IS_DBUS_CONNECTION (connection));
  g_assert (g_variant_is_object_path (object_path));
}

static void
valent_mock_application_plugin_shutdown (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));
}

static void
valent_mock_application_plugin_startup (ValentApplicationPlugin *plugin)
{
  g_assert (VALENT_IS_MOCK_APPLICATION_PLUGIN (plugin));
}

/*
 * GObject
 */
static void
valent_mock_application_plugin_class_init (ValentMockApplicationPluginClass *klass)
{
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_mock_application_plugin_enable;
  plugin_class->disable = valent_mock_application_plugin_disable;
  plugin_class->activate = valent_mock_application_plugin_activate;
  plugin_class->command_line = valent_mock_application_plugin_command_line;
  plugin_class->dbus_register = valent_mock_application_plugin_dbus_register;
  plugin_class->dbus_unregister = valent_mock_application_plugin_dbus_unregister;
  plugin_class->open = valent_mock_application_plugin_open;
  plugin_class->shutdown = valent_mock_application_plugin_shutdown;
  plugin_class->startup = valent_mock_application_plugin_startup;
}

static void
valent_mock_application_plugin_init (ValentMockApplicationPlugin *self)
{
}

