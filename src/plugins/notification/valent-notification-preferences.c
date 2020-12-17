// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-notifications.h>
#include <libvalent-ui.h>

#include "valent-notification-preferences.h"
#include "valent-notification-reply-dialog.h"


struct _ValentNotificationPreferences
{
  AdwPreferencesPage   parent_instance;

  GSettings           *settings;
  char                *plugin_context;

  /* Template Widgets */
  AdwPreferencesGroup *forward_group;
  GtkSwitch           *forward_notifications;
  GtkSwitch           *forward_when_active;

  AdwPreferencesGroup *application_group;
  GtkListBox          *application_list;
  GHashTable          *application_rows;
};

enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  N_PROPERTIES
};

/* Interfaces */
static void valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentNotificationPreferences, valent_notification_preferences, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_PLUGIN_PREFERENCES, valent_plugin_preferences_iface_init))

/*
 * Template Callbacks
 */
static void
on_row_activated (GtkWidget                     *widget,
                  ValentNotificationPreferences *self)
{
  GtkSwitch *sw;

  if ((sw = g_hash_table_lookup (self->application_rows, widget)) != NULL)
    gtk_switch_set_active (sw, !gtk_switch_get_active (sw));
}

static void
on_switch_toggled (GtkSwitch                     *sw,
                   GParamSpec                    *pspec,
                   ValentNotificationPreferences *self)
{
  GHashTableIter iter;
  gpointer row, row_switch;
  GVariantBuilder builder;
  GVariant *forward_deny;

  g_hash_table_iter_init (&iter, self->application_rows);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

  while (g_hash_table_iter_next (&iter, &row, &row_switch))
    {
      gboolean enabled;
      const char *name;

      if ((enabled = gtk_switch_get_active (GTK_SWITCH (row_switch))))
        return;

      name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
      g_variant_builder_add (&builder, "s", name);
    }

  forward_deny  = g_variant_builder_end (&builder);
  g_settings_set_value (self->settings, "forward-deny", forward_deny);
}

static void
add_application (ValentNotificationPreferences *self,
                 GVariant                      *app,
                 gboolean                       enabled)
{
  GtkWidget *row;
  GtkWidget *image;
  GtkWidget *sw;
  const char *title;
  g_autoptr (GVariant) iconv = NULL;
  g_autoptr (GIcon) icon = NULL;

  if (!g_variant_lookup (app, "name", "s", &title))
    return;

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "title",       title,
                      NULL);
  g_signal_connect (G_OBJECT (row),
                    "activated",
                    G_CALLBACK (on_row_activated),
                    self);

  /* App icon */
  if ((iconv = g_variant_lookup_value (app, "icon", NULL)) != NULL)
    icon = g_icon_deserialize (iconv);

  if (icon == NULL)
    icon = g_icon_new_for_string ("application-x-executable", NULL);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "gicon",     icon,
                        "icon-size", GTK_ICON_SIZE_LARGE,
                        NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  /* Enabled switch */
  sw = g_object_new (GTK_TYPE_SWITCH,
                     "active", enabled,
                     "valign", GTK_ALIGN_CENTER,
                     NULL);
  g_signal_connect (G_OBJECT (sw),
                    "notify::active",
                    G_CALLBACK (on_switch_toggled),
                    self);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), sw);

  g_hash_table_insert (self->application_rows, row, sw);
  gtk_list_box_insert (self->application_list, row, -1);
}

static void
populate_applications (ValentNotificationPreferences *self)
{
  GVariant *known;
  g_auto (GStrv) deny = NULL;
  GVariantIter iter;
  const char *key;
  GVariant *value;

  deny = g_settings_get_strv (self->settings, "forward-deny");

  /* Query the known applications */
  known = valent_notifications_get_applications (NULL);
  g_variant_iter_init (&iter, known);

  while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
    {
      gboolean enabled;

      enabled = !g_strv_contains ((const char * const *)deny, key);
      add_application (self, value, enabled);
    }
}

static void
on_reset_clicked (GtkButton                     *button,
                  ValentNotificationPreferences *self)
{
  GHashTableIter iter;
  gpointer row_switch;

  g_assert (GTK_IS_BUTTON (button));
  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));

  while (g_hash_table_iter_next (&iter, NULL, &row_switch))
    gtk_switch_set_active (GTK_SWITCH (row_switch), TRUE);

  g_settings_reset (self->settings, "applications");
}


/*
 * ValentPluginPreferences
 */
static void
valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface)
{
}


/*
 * GObject
 */
static void
valent_notification_preferences_constructed (GObject *object)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);

  /* Setup GSettings */
  self->settings = valent_device_plugin_new_settings (self->plugin_context,
                                                      "notification");

  /* Forwarding Settings */
  g_settings_bind (self->settings,              "forward-notifications",
                   self->forward_notifications, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->settings,            "forward-when-active",
                   self->forward_when_active, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Applications */
  gtk_list_box_set_sort_func (self->application_list,
                              valent_plugin_preferences_row_sort,
                              self, NULL);
  populate_applications (self);

  G_OBJECT_CLASS (valent_notification_preferences_parent_class)->constructed (object);
}

static void
valent_notification_preferences_finalize (GObject *object)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);

  g_clear_pointer (&self->application_rows, g_hash_table_unref);
  g_clear_pointer (&self->plugin_context, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_notification_preferences_parent_class)->finalize (object);
}

static void
valent_notification_preferences_get_property (GObject    *object,
                                              guint       prop_id,
                                              GValue     *value,
                                              GParamSpec *pspec)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      g_value_set_string (value, self->plugin_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_preferences_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      self->plugin_context = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_preferences_class_init (ValentNotificationPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_notification_preferences_constructed;
  object_class->finalize = valent_notification_preferences_finalize;
  object_class->get_property = valent_notification_preferences_get_property;
  object_class->set_property = valent_notification_preferences_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/notification/valent-notification-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_group);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_notifications);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_when_active);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_group);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_list);

  g_object_class_override_property (object_class,
                                    PROP_PLUGIN_CONTEXT,
                                    "plugin-context");
}

static void
valent_notification_preferences_init (ValentNotificationPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->application_rows = g_hash_table_new (NULL, NULL);
}

