// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences-window"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-device-preferences-group.h"
#include "valent-device-preferences-window.h"


struct _ValentDevicePreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

  ValentDevice         *device;
  GHashTable           *plugins;

  /* template */
  AdwPreferencesPage   *status_page;
  AdwPreferencesPage   *sync_page;
  AdwPreferencesPage   *other_page;
  AdwPreferencesPage   *plugin_page;
  AdwPreferencesGroup  *plugin_group;
  GtkListBox           *plugin_list;
};

G_DEFINE_FINAL_TYPE (ValentDevicePreferencesWindow, valent_device_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static int
plugin_list_sort (GtkListBoxRow *row1,
                  GtkListBoxRow *row2,
                  gpointer       user_data)
{
  if G_UNLIKELY (!ADW_IS_PREFERENCES_ROW (row1) ||
                 !ADW_IS_PREFERENCES_ROW (row2))
    return 0;

  return g_utf8_collate (adw_preferences_row_get_title ((AdwPreferencesRow *)row1),
                         adw_preferences_row_get_title ((AdwPreferencesRow *)row2));
}

/*
 * Plugin Callbacks
 */
typedef struct
{
  AdwPreferencesWindow *window;
  AdwPreferencesPage   *page;
  AdwPreferencesGroup  *group;
  GtkWidget            *row;
} PluginData;

static void
plugin_data_free (gpointer data)
{
  PluginData *plugin = (PluginData *)data;
  ValentDevicePreferencesWindow *self = VALENT_DEVICE_PREFERENCES_WINDOW (plugin->window);

  g_assert (VALENT_IS_DEVICE_PREFERENCES_WINDOW (self));

  if (plugin->page != NULL && plugin->group != NULL)
    adw_preferences_page_remove (plugin->page, plugin->group);

  if (plugin->row != NULL)
    gtk_list_box_remove (self->plugin_list, plugin->row);

  g_free (plugin);
}

static void
valent_device_preferences_window_add_plugin (ValentDevicePreferencesWindow *self,
                                             const char                    *module)
{
  ValentContext *context = NULL;
  g_autoptr (ValentContext) plugin_context = NULL;
  g_autoptr (GSettings) settings = NULL;
  PeasEngine *engine;
  PeasPluginInfo *info;
  PluginData *plugin;
  const char *title;
  const char *subtitle;
  const char *icon_name;

  g_assert (VALENT_IS_DEVICE_PREFERENCES_WINDOW (self));
  g_assert (module != NULL && *module != '\0');

  engine = valent_get_plugin_engine ();
  info = peas_engine_get_plugin_info (engine, module);
  plugin = g_new0 (PluginData, 1);
  plugin->window = ADW_PREFERENCES_WINDOW (self);

  title = peas_plugin_info_get_name (info);
  subtitle = peas_plugin_info_get_description (info);
  icon_name = peas_plugin_info_get_icon_name (info);

  /* Plugin Row */
  plugin->row = g_object_new (ADW_TYPE_SWITCH_ROW,
                              "title",      title,
                              "subtitle",   subtitle,
                              "selectable", FALSE,
                              NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (plugin->row),
                             gtk_image_new_from_icon_name (icon_name));
  gtk_list_box_insert (self->plugin_list, plugin->row, -1);

  /* Plugin Toggle */
  context = valent_device_get_context (self->device);
  plugin_context = valent_context_get_plugin_context (context, info);
  settings = valent_context_create_settings (plugin_context,
                                             "ca.andyholmes.Valent.Plugin");
  g_settings_bind (settings,    "enabled",
                   plugin->row, "active",
                   G_SETTINGS_BIND_DEFAULT);
  adw_switch_row_set_active (ADW_SWITCH_ROW (plugin->row),
                             g_settings_get_boolean (settings, "enabled"));
  g_object_set_data_full (G_OBJECT (plugin->row),
                          "valent-plugin-settings",
                          g_steal_pointer (&settings),
                          g_object_unref);

  /* Preferences Page */
  if (peas_engine_provides_extension (engine,
                                      info,
                                      VALENT_TYPE_DEVICE_PREFERENCES_GROUP))
    {
      GObject *group;
      const char *category;

      group = peas_engine_create_extension (engine,
                                            info,
                                            VALENT_TYPE_DEVICE_PREFERENCES_GROUP,
                                            "context",     plugin_context,
                                            "name",        module,
                                            "title",       title,
                                            "description", subtitle,
                                            NULL);

      g_return_if_fail (VALENT_IS_DEVICE_PREFERENCES_GROUP (group));
      plugin->group = ADW_PREFERENCES_GROUP (group);

      category = peas_plugin_info_get_external_data (info,
                                                     "X-DevicePluginCategory");

      if (g_strcmp0 (category, "Network;FileTransfer;") == 0 ||
          g_strcmp0 (category, "Network;RemoteAccess;") == 0)
        plugin->page = self->sync_page;
      else if (g_strcmp0 (category, "System;Monitor;") == 0 ||
          g_strcmp0 (category, "Network;Telephony;") == 0)
        plugin->page = self->status_page;
      else
        plugin->page = self->other_page;

      adw_preferences_page_add (plugin->page, plugin->group);
    }

  g_hash_table_replace (self->plugins,
                        g_strdup (module),
                        g_steal_pointer (&plugin));
}

static int
plugin_sort (gconstpointer a,
             gconstpointer b)
{
  const char *a_ = *(char **)a;
  const char *b_ = *(char **)b;

  return strcmp (a_, b_);
}

static void
on_plugins_changed (ValentDevice                  *device,
                    GParamSpec                    *pspec,
                    ValentDevicePreferencesWindow *self)
{
  g_auto (GStrv) plugins = NULL;
  GHashTableIter iter;
  const char *module;

  plugins = valent_device_get_plugins (device);
  qsort (plugins, g_strv_length (plugins), sizeof (char *), plugin_sort);

  /* Remove */
  g_hash_table_iter_init (&iter, self->plugins);

  while (g_hash_table_iter_next (&iter, (void **)&module, NULL))
    {
      if (!g_strv_contains ((const char * const *)plugins, module))
        g_hash_table_iter_remove (&iter);
    }

  for (unsigned int i = 0; plugins[i] != NULL; i++)
    {
      if (!g_hash_table_contains (self->plugins, plugins[i]))
        valent_device_preferences_window_add_plugin (self, plugins[i]);
    }
}

/*
 * GObject
 */
static void
valent_device_preferences_window_constructed (GObject *object)
{
  ValentDevicePreferencesWindow *self = VALENT_DEVICE_PREFERENCES_WINDOW (object);

  /* Device */
  g_object_bind_property (self->device, "name",
                          self,         "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);

  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "device",
                                  G_ACTION_GROUP (self->device));

  /* Device_plugins */
  g_signal_connect_object (self->device,
                           "notify::plugins",
                           G_CALLBACK (on_plugins_changed),
                           self, 0);
  on_plugins_changed (self->device, NULL, self);

  G_OBJECT_CLASS (valent_device_preferences_window_parent_class)->constructed (object);
}

static void
valent_device_preferences_window_dispose (GObject *object)
{
  ValentDevicePreferencesWindow *self = VALENT_DEVICE_PREFERENCES_WINDOW (object);

  g_clear_object (&self->device);
  g_clear_pointer (&self->plugins, g_hash_table_unref);

  gtk_widget_dispose_template (GTK_WIDGET (object),
                               VALENT_TYPE_DEVICE_PREFERENCES_WINDOW);

  G_OBJECT_CLASS (valent_device_preferences_window_parent_class)->dispose (object);
}

static void
valent_device_preferences_window_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  ValentDevicePreferencesWindow *self = VALENT_DEVICE_PREFERENCES_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_window_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  ValentDevicePreferencesWindow *self = VALENT_DEVICE_PREFERENCES_WINDOW (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_window_class_init (ValentDevicePreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_device_preferences_window_constructed;
  object_class->dispose = valent_device_preferences_window_dispose;
  object_class->get_property = valent_device_preferences_window_get_property;
  object_class->set_property = valent_device_preferences_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-device-preferences-window.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, status_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, sync_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, other_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, plugin_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, plugin_group);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, plugin_list);

  /**
   * ValentDevicePreferencesWindow:device:
   *
   * The device this panel controls and represents.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_device_preferences_window_init (ValentDevicePreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->plugin_list, plugin_list_sort, NULL, NULL);
  self->plugins = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         plugin_data_free);
}

