// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-core.h>

#include "valent-mock-application-plugin.h"


struct _ValentMockApplicationPlugin
{
  ValentApplicationPlugin  parent_instance;
};

G_DEFINE_TYPE (ValentMockApplicationPlugin, valent_mock_application_plugin, VALENT_TYPE_APPLICATION_PLUGIN)


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
  g_assert (VALENT_IS_APPLICATION_PLUGIN (plugin));
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
  plugin_class->open = valent_mock_application_plugin_open;
}

static void
valent_mock_application_plugin_init (ValentMockApplicationPlugin *self)
{
}

