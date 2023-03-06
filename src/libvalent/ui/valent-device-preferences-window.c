// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences-window"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-device.h>

#include "valent-device-preferences-page.h"
#include "valent-device-preferences-window.h"
#include "valent-preferences-window.h"


struct _ValentDevicePreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

  ValentDevice         *device;
  GHashTable           *plugins;

  /* template */
  AdwPreferencesPage   *main_page;
  AdwPreferencesGroup  *plugin_group;
  GtkListBox           *plugin_list;
  AdwPreferencesGroup  *unpair_group;
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
  GtkWidget            *row;
} PluginData;

static void
plugin_data_free (gpointer data)
{
  PluginData *plugin = (PluginData *)data;
  ValentDevicePreferencesWindow *self = VALENT_DEVICE_PREFERENCES_WINDOW (plugin->window);

  g_assert (VALENT_IS_DEVICE_PREFERENCES_WINDOW (self));

  if (plugin->page != NULL)
    adw_preferences_window_remove (plugin->window, plugin->page);

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
  GtkWidget *sw, *separator, *button;

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
  plugin->row = g_object_new (ADW_TYPE_ACTION_ROW,
                              "icon-name", icon_name,
                              "title",     title,
                              "subtitle",  subtitle,
                              NULL);

  sw = g_object_new (GTK_TYPE_SWITCH,
                     "active",  TRUE,
                     "valign",  GTK_ALIGN_CENTER,
                     NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (plugin->row), sw);

  separator = g_object_new (GTK_TYPE_SEPARATOR,
                            "margin-bottom", 8,
                            "margin-top",    8,
                            "orientation",   GTK_ORIENTATION_VERTICAL,
                            NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (plugin->row), separator);

  button = g_object_new (GTK_TYPE_BUTTON,
                               "icon-name", "emblem-system-symbolic",
                               "sensitive", FALSE,
                               "valign",    GTK_ALIGN_CENTER,
                               NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (plugin->row), button);

  gtk_list_box_insert (self->plugin_list, plugin->row, -1);

  /* Plugin Toggle */
  context = valent_device_get_context (self->device);
  plugin_context = valent_context_get_plugin_context (context, info);
  settings = valent_context_create_settings (plugin_context,
                                             "ca.andyholmes.Valent.Plugin");
  g_settings_bind (settings, "enabled",
                   sw,       "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_set_data_full (G_OBJECT (plugin->row),
                          "plugin-settings",
                          g_steal_pointer (&settings),
                          g_object_unref);

  /* Preferences Page */
  if (peas_engine_provides_extension (engine,
                                      info,
                                      VALENT_TYPE_DEVICE_PREFERENCES_PAGE))
    {
      PeasExtension *page;

      page = peas_engine_create_extension (engine,
                                           info,
                                           VALENT_TYPE_DEVICE_PREFERENCES_PAGE,
                                           "context",   plugin_context,
                                           "name",      module,
                                           "icon-name", icon_name,
                                           "title",     title,
                                           NULL);
      adw_preferences_window_add (ADW_PREFERENCES_WINDOW (self),
                                  ADW_PREFERENCES_PAGE (page));
      plugin->page = ADW_PREFERENCES_PAGE (page);

      g_object_set (button,
                    "action-target", g_variant_new_string (module),
                    "action-name",   "win.page",
                    "sensitive",     TRUE,
                    NULL);
    }

  g_hash_table_replace (self->plugins,
                        g_strdup (module),
                        g_steal_pointer (&plugin));
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

static gboolean
device_state_transform_to (GBinding     *binding,
                           const GValue *from_value,
                           GValue       *to_value,
                           gpointer      user_data)
{
  ValentDeviceState state = g_value_get_flags (from_value);

  g_value_set_boolean (to_value, (state & VALENT_DEVICE_STATE_PAIRED) != 0);

  return TRUE;
}

/*
 * GActions
 */
static void
page_action (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  AdwPreferencesWindow *window = ADW_PREFERENCES_WINDOW (widget);
  const char *module;

  module = g_variant_get_string (parameter, NULL);
  adw_preferences_window_set_visible_page_name (window, module);
}

static void
previous_action (GtkWidget  *widget,
                 const char *action_name,
                 GVariant   *parameter)
{
  AdwPreferencesWindow *window = ADW_PREFERENCES_WINDOW (widget);
  const char *page_name;

  page_name = adw_preferences_window_get_visible_page_name (window);

  if (g_strcmp0 (page_name, "main") == 0)
    gtk_window_destroy (GTK_WINDOW (window));
  else
    adw_preferences_window_set_visible_page_name (window, "main");
}

/*
 * GObject
 */
static void
valent_device_preferences_window_constructed (GObject *object)
{
  ValentDevicePreferencesWindow *self = VALENT_DEVICE_PREFERENCES_WINDOW (object);

  /* Modify the dialog */
  if (!valent_preferences_window_modify (ADW_PREFERENCES_WINDOW (self)))
    g_warning ("Failed to modify AdwPreferencesWindow");

  /* Device */
  g_object_bind_property (self->device, "name",
                          self,         "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  g_object_bind_property_full (self->device,       "state",
                               self->unpair_group, "visible",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                               device_state_transform_to, NULL,
                               NULL, NULL);

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
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, main_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, plugin_group);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, plugin_list);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesWindow, unpair_group);

  gtk_widget_class_install_action (widget_class, "win.page", "s", page_action);
  gtk_widget_class_install_action (widget_class, "win.previous", NULL, previous_action);

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

