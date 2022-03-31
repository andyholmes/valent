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

/*
 * GObject
 */
static void
valent_mock_application_plugin_class_init (ValentMockApplicationPluginClass *klass)
{
  ValentApplicationPluginClass *plugin_class = VALENT_APPLICATION_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_mock_application_plugin_enable;
  plugin_class->disable = valent_mock_application_plugin_disable;
}

static void
valent_mock_application_plugin_init (ValentMockApplicationPlugin *self)
{
}

