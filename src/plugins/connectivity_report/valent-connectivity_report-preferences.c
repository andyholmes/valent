// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-connectivity_report-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-connectivity_report-preferences.h"


struct _ValentConnectivityReportPreferences
{
  AdwPreferencesPage  parent_instance;

  char               *device_id;
  PeasPluginInfo     *plugin_info;
  GSettings          *settings;

  /* Template widgets */
  AdwActionRow       *share_state_row;
  GtkSwitch          *share_state;

  AdwActionRow       *offline_notification_row;
  GtkSwitch          *offline_notification;
};

/* Interfaces */
static void valent_device_preferences_page_iface_init (ValentDevicePreferencesPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentConnectivityReportPreferences, valent_connectivity_report_preferences, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PREFERENCES_PAGE, valent_device_preferences_page_iface_init))


enum {
  PROP_0,
  PROP_DEVICE_ID,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};


/*
 * ValentDevicePreferencesPage
 */
static void
valent_device_preferences_page_iface_init (ValentDevicePreferencesPageInterface *iface)
{
}

/*
 * GObject
 */
static void
valent_connectivity_report_preferences_constructed (GObject *object)
{
  ValentConnectivityReportPreferences *self = VALENT_CONNECTIVITY_REPORT_PREFERENCES (object);

  /* Setup GSettings */
  self->settings = valent_device_plugin_create_settings (self->plugin_info,
                                                         self->device_id);

  g_settings_bind (self->settings,    "share-state",
                   self->share_state, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings,             "offline-notification",
                   self->offline_notification, "active",
                   G_SETTINGS_BIND_DEFAULT);

  G_OBJECT_CLASS (valent_connectivity_report_preferences_parent_class)->constructed (object);
}

static void
valent_connectivity_report_preferences_finalize (GObject *object)
{
  ValentConnectivityReportPreferences *self = VALENT_CONNECTIVITY_REPORT_PREFERENCES (object);

  g_clear_pointer (&self->device_id, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_connectivity_report_preferences_parent_class)->finalize (object);
}

static void
valent_connectivity_report_preferences_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentConnectivityReportPreferences *self = VALENT_CONNECTIVITY_REPORT_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      g_value_set_string (value, self->device_id);
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, self->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_connectivity_report_preferences_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentConnectivityReportPreferences *self = VALENT_CONNECTIVITY_REPORT_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_DEVICE_ID:
      self->device_id = g_value_dup_string (value);
      break;

    case PROP_PLUGIN_INFO:
      self->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_connectivity_report_preferences_class_init (ValentConnectivityReportPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_connectivity_report_preferences_constructed;
  object_class->finalize = valent_connectivity_report_preferences_finalize;
  object_class->get_property = valent_connectivity_report_preferences_get_property;
  object_class->set_property = valent_connectivity_report_preferences_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/connectivity_report/valent-connectivity_report-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, share_state_row);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, share_state);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, offline_notification_row);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectivityReportPreferences, offline_notification);

  g_object_class_override_property (object_class, PROP_DEVICE_ID, "device-id");
  g_object_class_override_property (object_class, PROP_PLUGIN_INFO, "plugin-info");
}

static void
valent_connectivity_report_preferences_init (ValentConnectivityReportPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

