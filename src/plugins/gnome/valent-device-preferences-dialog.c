// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences-dialog"

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

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

static GtkWidget *
create_plugin_row (gpointer item,
                   gpointer user_data)
{
  ValentDevicePreferencesDialog *self = VALENT_DEVICE_PREFERENCES_DIALOG (user_data);
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  const char *module;
  ValentContext *context = NULL;
  g_autoptr (ValentContext) plugin_context = NULL;

  engine = valent_get_plugin_engine ();
  module = gtk_string_object_get_string (GTK_STRING_OBJECT (item));
  plugin_info = peas_engine_get_plugin_info (engine, module);
  context = valent_device_get_context (self->device);
  plugin_context = valent_context_get_plugin_context (context, plugin_info);

  return g_object_new (VALENT_TYPE_PLUGIN_ROW,
                       "context",     plugin_context,
                       "plugin-info", plugin_info,
                       "title",       peas_plugin_info_get_name (plugin_info),
                       "subtitle",    peas_plugin_info_get_description (plugin_info),
                       NULL);
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
  g_autoptr (GtkStringList) model = NULL;

  plugins = valent_device_get_plugins (device);
  qsort (plugins, g_strv_length (plugins), sizeof (char *), plugin_sort);
  model = gtk_string_list_new ((const char * const *)plugins);
  gtk_list_box_bind_model (self->plugin_list,
                           G_LIST_MODEL (model),
                           create_plugin_row,
                           self, NULL);
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
                           self,
                           G_CONNECT_DEFAULT);
  on_plugins_changed (self->device, NULL, self);
}

static void
valent_device_preferences_dialog_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_DEVICE_PREFERENCES_DIALOG);

  G_OBJECT_CLASS (valent_device_preferences_dialog_parent_class)->dispose (object);
}

static void
valent_device_preferences_dialog_finalize (GObject *object)
{
  ValentDevicePreferencesDialog *self = VALENT_DEVICE_PREFERENCES_DIALOG (object);

  g_clear_object (&self->device);

  G_OBJECT_CLASS (valent_device_preferences_dialog_parent_class)->finalize (object);
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
  object_class->finalize = valent_device_preferences_dialog_finalize;
  object_class->get_property = valent_device_preferences_dialog_get_property;
  object_class->set_property = valent_device_preferences_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-device-preferences-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, status_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, sync_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, other_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, plugin_page);
  gtk_widget_class_bind_template_child (widget_class, ValentDevicePreferencesDialog, plugin_list);

  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_device_preferences_dialog_init (ValentDevicePreferencesDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

