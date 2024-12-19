// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences-dialog"

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-device-preferences-battery.h"
#include "valent-device-preferences-clipboard.h"
#include "valent-device-preferences-commands.h"
#include "valent-device-preferences-connectivity.h"
#include "valent-device-preferences-contacts.h"
#include "valent-device-preferences-notification.h"
#include "valent-device-preferences-sftp.h"
#include "valent-device-preferences-share.h"
#include "valent-device-preferences-telephony.h"
#include "valent-device-preferences-group.h"
#include "valent-plugin-row.h"

#include "valent-device-preferences-dialog.h"


struct _ValentDevicePreferencesDialog
{
  AdwPreferencesDialog  parent_instance;

  ValentDevice         *device;
  GHashTable           *plugins;

  /* template */
  AdwPreferencesPage   *status_page;
  AdwPreferencesPage   *sync_page;
  AdwPreferencesPage   *other_page;
  AdwPreferencesPage   *plugin_page;
  GtkListBox           *plugin_list;
};

G_DEFINE_FINAL_TYPE (ValentDevicePreferencesDialog, valent_device_preferences_dialog, ADW_TYPE_PREFERENCES_DIALOG)

typedef enum {
  PROP_DEVICE = 1,
} ValentDevicePreferencesDialogProperty;

static GParamSpec *properties[PROP_DEVICE + 1] = { NULL, };


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
  AdwPreferencesDialog *window;
  AdwPreferencesPage   *page;
  AdwPreferencesGroup  *group;
  GtkWidget            *row;
} PluginData;

static void
plugin_data_free (gpointer data)
{
  PluginData *plugin = (PluginData *)data;
  ValentDevicePreferencesDialog *self = VALENT_DEVICE_PREFERENCES_DIALOG (plugin->window);

  g_assert (VALENT_IS_DEVICE_PREFERENCES_DIALOG (self));

  if (plugin->page != NULL && plugin->group != NULL)
    adw_preferences_page_remove (plugin->page, plugin->group);

  if (plugin->row != NULL)
    gtk_list_box_remove (self->plugin_list, plugin->row);

  g_free (plugin);
}

static void
valent_device_preferences_dialog_add_plugin (ValentDevicePreferencesDialog *self,
                                             const char                    *module)
{
  PeasPluginInfo *info;
  PluginData *plugin;
  const char *title;
  const char *subtitle;

  g_assert (VALENT_IS_DEVICE_PREFERENCES_DIALOG (self));
  g_assert (module != NULL && *module != '\0');

  info = peas_engine_get_plugin_info (valent_get_plugin_engine (), module);
  title = peas_plugin_info_get_name (info);
  subtitle = peas_plugin_info_get_description (info);

  plugin = g_new0 (PluginData, 1);
  plugin->window = ADW_PREFERENCES_DIALOG (self);

  /* Plugin Row
   */
  plugin->row = g_object_new (VALENT_TYPE_PLUGIN_ROW,
                              "data-source",   self->device,
                              "plugin-domain", "device",
                              "plugin-info",   info,
                              "title",         title,
                              "subtitle",      subtitle,
                              NULL);
  gtk_list_box_insert (self->plugin_list, plugin->row, -1);

  /* Preferences Page
   */
  struct
  {
    const char *name;
    const char *category;
    GType       type;
  } preferences[] = {
    {
      .name = "battery",
      .category = "status",
      .type = VALENT_TYPE_BATTERY_PREFERENCES,
    },
    {
      .name = "connectivity_report",
      .category = "status",
      .type = VALENT_TYPE_CONNECTIVITY_REPORT_PREFERENCES,
    },
    {
      .name = "telephony",
      .category = "status",
      .type = VALENT_TYPE_TELEPHONY_PREFERENCES,
    },

    {
      .name = "clipboard",
      .category = "sync",
      .type = VALENT_TYPE_CLIPBOARD_PREFERENCES,
    },
    {
      .name = "contacts",
      .category = "sync",
      .type = VALENT_TYPE_CONTACTS_PREFERENCES,
    },
    {
      .name = "notification",
      .category = "sync",
      .type = VALENT_TYPE_NOTIFICATION_PREFERENCES,
    },
    {
      .name = "sftp",
      .category = "sync",
      .type = VALENT_TYPE_SFTP_PREFERENCES,
    },

    {
      .name = "runcommand",
      .category = "other",
      .type = VALENT_TYPE_RUNCOMMAND_PREFERENCES,
    },
    {
      .name = "share",
      .category = "other",
      .type = VALENT_TYPE_SHARE_PREFERENCES,
    },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (preferences); i++)
    {
      if (g_str_equal (preferences[i].name, module))
        {
          plugin->group = g_object_new (preferences[i].type,
                                        "data-source", self->device,
                                        "plugin-info", info,
                                        "name",        module,
                                        "title",       title,
                                        "description", subtitle,
                                        NULL);

          if (g_str_equal (preferences[i].category, "status"))
            plugin->page = self->status_page;
          else if (g_str_equal (preferences[i].category, "sync"))
            plugin->page = self->sync_page;
          else
            plugin->page = self->other_page;

          adw_preferences_page_add (plugin->page, plugin->group);
          break;
        }
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
                    ValentDevicePreferencesDialog *self)
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
        valent_device_preferences_dialog_add_plugin (self, plugins[i]);
    }
}

/*
 * GObject
 */
static void
valent_device_preferences_dialog_constructed (GObject *object)
{
  ValentDevicePreferencesDialog *self = VALENT_DEVICE_PREFERENCES_DIALOG (object);

  G_OBJECT_CLASS (valent_device_preferences_dialog_parent_class)->constructed (object);

  g_object_bind_property (self->device, "name",
                          self,         "title",
                          G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE);
  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "device",
                                  G_ACTION_GROUP (self->device));

  g_signal_connect_object (self->device,
                           "notify::plugins",
                           G_CALLBACK (on_plugins_changed),
                           self, 0);
  on_plugins_changed (self->device, NULL, self);
}

static void
valent_device_preferences_dialog_dispose (GObject *object)
{
  ValentDevicePreferencesDialog *self = VALENT_DEVICE_PREFERENCES_DIALOG (object);

  g_clear_object (&self->device);
  g_clear_pointer (&self->plugins, g_hash_table_unref);

  gtk_widget_dispose_template (GTK_WIDGET (object),
                               VALENT_TYPE_DEVICE_PREFERENCES_DIALOG);

  G_OBJECT_CLASS (valent_device_preferences_dialog_parent_class)->dispose (object);
}

static void
valent_device_preferences_dialog_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  ValentDevicePreferencesDialog *self = VALENT_DEVICE_PREFERENCES_DIALOG (object);

  switch ((ValentDevicePreferencesDialogProperty)prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_dialog_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  ValentDevicePreferencesDialog *self = VALENT_DEVICE_PREFERENCES_DIALOG (object);

  switch ((ValentDevicePreferencesDialogProperty)prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_device_preferences_dialog_class_init (ValentDevicePreferencesDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_device_preferences_dialog_constructed;
  object_class->dispose = valent_device_preferences_dialog_dispose;
  object_class->get_property = valent_device_preferences_dialog_get_property;
  object_class->set_property = valent_device_preferences_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-device-preferences-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, status_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, sync_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, other_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, plugin_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, plugin_list);

  /**
   * ValentDevicePreferencesDialog:device:
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

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_device_preferences_dialog_init (ValentDevicePreferencesDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->plugin_list, plugin_list_sort, NULL, NULL);
  self->plugins = g_hash_table_new_full (g_str_hash,
                                         g_str_equal,
                                         g_free,
                                         plugin_data_free);
}

