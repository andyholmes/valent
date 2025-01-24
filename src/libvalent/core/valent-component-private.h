// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "valent-component.h"
#include "valent-context.h"

#define VALENT_PLUGIN_SCHEMA "ca.andyholmes.Valent.Plugin"

G_BEGIN_DECLS

/*< private >
 * ValentPlugin:
 * @parent: (type GObject.Object): the owner of the plugin
 * @context: the plugin context
 * @info: the plugin info
 * @extension: (type Valent.Extension): the plugin extension
 * @cancellable: the initialization cancellable
 * @settings: the plugin settings
 *
 * A `struct` for representing a plugin pod.
 */
typedef struct
{
  gpointer        parent;
  ValentContext  *context;
  PeasPluginInfo *info;
  gpointer        extension;
  GCancellable   *cancellable;

  /*< private >*/
  GSettings      *settings;
} ValentPlugin;


/*< private >
 * valent_plugin_new:
 * @parent: (type GObject.Object): the parent
 * @parent_context: the parent context
 * @info: the plugin info
 *
 * Create a new `ValentPlugin`.
 *
 * Returns: (transfer full): a new `ValentPlugin`
 */
static inline ValentPlugin *
valent_plugin_new (gpointer        parent,
                   ValentContext  *parent_context,
                   PeasPluginInfo *info,
                   GCallback       enable_func)
{
  ValentPlugin *plugin = NULL;

  g_assert (G_IS_OBJECT (parent));
  g_assert (VALENT_IS_CONTEXT (parent_context));
  g_assert (info != NULL);

  plugin = g_new0 (ValentPlugin, 1);
  plugin->parent = parent;
  plugin->info = g_object_ref (info);
  plugin->context = valent_context_get_plugin_context (parent_context, info);
  plugin->settings = valent_context_create_settings (plugin->context,
                                                     VALENT_PLUGIN_SCHEMA);

  g_signal_connect_swapped (plugin->settings,
                            "changed::enabled",
                            enable_func,
                            plugin);

  return g_steal_pointer (&plugin);
}

/*< private >
 * valent_plugin_get_enabled:
 * @data: a `ValentPlugin`
 *
 * Get whether the plugin is enabled.
 *
 * Returns: %TRUE if enabled, %FALSE if not
 */
static inline gboolean
valent_plugin_get_enabled (ValentPlugin *plugin)
{
  g_assert (plugin != NULL);

  return g_settings_get_boolean (plugin->settings, "enabled");
}

/*< private >
 * valent_plugin_free:
 * @data: a `ValentPlugin`
 *
 * Free a `ValentPlugin`.
 */
static inline void
valent_plugin_free (gpointer data)
{
  ValentPlugin *plugin = (ValentPlugin *)data;

  g_assert (data != NULL);

  g_cancellable_cancel (plugin->cancellable);
  g_signal_handlers_disconnect_by_data (plugin->settings, plugin);

  if (VALENT_IS_OBJECT (plugin->extension))
    {
      valent_object_destroy (VALENT_OBJECT (plugin->extension));
      g_clear_object (&plugin->extension);
    }

  plugin->parent = NULL;
  g_clear_object (&plugin->info);
  g_clear_object (&plugin->cancellable);
  g_clear_object (&plugin->context);
  g_clear_object (&plugin->extension);
  g_clear_object (&plugin->settings);
  g_clear_pointer (&plugin, g_free);
}

G_END_DECLS

