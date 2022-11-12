// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-notifications.h>
#include <libvalent-ui.h>

#include "valent-notification-preferences.h"


struct _ValentNotificationPreferences
{
  ValentDevicePreferencesPage  parent_instance;

  /* template */
  AdwPreferencesGroup         *forward_group;
  GtkSwitch                   *forward_notifications;
  GtkSwitch                   *forward_when_active;

  AdwPreferencesGroup         *application_group;
  GtkListBox                  *application_list;
  GHashTable                  *application_rows;
};

G_DEFINE_TYPE (ValentNotificationPreferences, valent_notification_preferences, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


static int
application_list_sort (GtkListBoxRow *row1,
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
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;
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

  forward_deny = g_variant_builder_end (&builder);
  settings = valent_device_preferences_page_get_settings (page);
  g_settings_set_value (settings, "forward-deny", forward_deny);
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

  if (!g_variant_lookup (app, "name", "&s", &title))
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
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;
  GVariant *known;
  g_auto (GStrv) deny = NULL;
  GVariantIter iter;
  const char *key;
  GVariant *value;

  settings = valent_device_preferences_page_get_settings (page);
  deny = g_settings_get_strv (settings, "forward-deny");

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
reset_action (GtkWidget  *widget,
              const char *action,
              GVariant   *parameter)
{
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (widget);
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (page);
  GSettings *settings;
  GHashTableIter iter;
  gpointer row_switch;

  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));
  g_assert (VALENT_IS_DEVICE_PREFERENCES_PAGE (self));

  g_hash_table_iter_init (&iter, self->application_rows);

  while (g_hash_table_iter_next (&iter, NULL, &row_switch))
    gtk_switch_set_active (GTK_SWITCH (row_switch), TRUE);

  settings = valent_device_preferences_page_get_settings (page);
  g_settings_reset (settings, "applications");
}


/*
 * GObject
 */
static void
valent_notification_preferences_constructed (GObject *object)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;

  /* Setup GSettings */
  settings = valent_device_preferences_page_get_settings (page);

  g_settings_bind (settings,                    "forward-notifications",
                   self->forward_notifications, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (settings,                  "forward-when-active",
                   self->forward_when_active, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Applications */
  gtk_list_box_set_sort_func (self->application_list,
                              application_list_sort,
                              self, NULL);
  populate_applications (self);

  G_OBJECT_CLASS (valent_notification_preferences_parent_class)->constructed (object);
}

static void
valent_notification_preferences_finalize (GObject *object)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);

  g_clear_pointer (&self->application_rows, g_hash_table_unref);

  G_OBJECT_CLASS (valent_notification_preferences_parent_class)->finalize (object);
}

static void
valent_notification_preferences_class_init (ValentNotificationPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_notification_preferences_constructed;
  object_class->finalize = valent_notification_preferences_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/notification/valent-notification-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_group);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_notifications);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_when_active);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_group);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_list);
  gtk_widget_class_install_action (widget_class, "preferences.reset", NULL, reset_action);
}

static void
valent_notification_preferences_init (ValentNotificationPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->application_rows = g_hash_table_new (NULL, NULL);
}

