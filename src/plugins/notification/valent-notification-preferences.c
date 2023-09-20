// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-notification-preferences.h"


struct _ValentNotificationPreferences
{
  ValentDevicePreferencesGroup  parent_instance;

  /* template */
  AdwExpanderRow               *forward_notifications;
  GtkSwitch                    *forward_when_active;

  AdwNavigationPage            *application_page;
  GtkStack                     *application_title_stack;
  GtkToggleButton              *application_filter_button;
  GtkSearchEntry               *application_filter_entry;
  AdwActionRow                 *application_row;
  GtkListBox                   *application_list;

  char                         *application_filter;
  GHashTable                   *application_rows;
};

G_DEFINE_FINAL_TYPE (ValentNotificationPreferences, valent_notification_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


static gboolean
application_list_filter (GtkListBoxRow *row,
                         gpointer       user_data)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (user_data);
  g_autofree char *haystack = NULL;
  const char *title = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));

  if (self->application_filter == NULL || *self->application_filter == '\0')
    return TRUE;

  title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  haystack = g_utf8_casefold (title, -1);

  return g_strrstr (haystack, self->application_filter) != NULL;
}

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
on_search_changed (GtkSearchEntry                *entry,
                   ValentNotificationPreferences *self)
{
  g_autofree char *query = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));

  query = g_utf8_casefold (gtk_editable_get_text (GTK_EDITABLE (entry)), -1);

  if (g_set_str (&self->application_filter, query))
    gtk_list_box_invalidate_filter (self->application_list);
}

static void
on_search_toggled (GtkToggleButton               *button,
                   GParamSpec                    *pspec,
                   ValentNotificationPreferences *self)
{
  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));

  if (gtk_toggle_button_get_active (button))
    {
      gtk_stack_set_visible_child_name (self->application_title_stack, "search");
      gtk_widget_grab_focus (GTK_WIDGET (self->application_filter_entry));
      gtk_editable_set_position (GTK_EDITABLE (self->application_filter_entry), -1);
    }
  else
    {
      gtk_editable_set_text (GTK_EDITABLE (self->application_filter_entry), "");
      gtk_stack_set_visible_child_name (self->application_title_stack, "title");
    }
}

static void
on_search_started (GtkSearchEntry                *entry,
                   ValentNotificationPreferences *self)
{
  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));

  gtk_toggle_button_set_active (self->application_filter_button, TRUE);
}

static void
on_stop_search (GtkSearchEntry                *entry,
                ValentNotificationPreferences *self)
{
  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));

  gtk_toggle_button_set_active (self->application_filter_button, FALSE);
}

static void
on_switch_toggled (GObject                       *object,
                   GParamSpec                    *pspec,
                   ValentNotificationPreferences *self)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  GHashTableIter iter;
  gpointer row;
  GVariantBuilder builder;
  GVariant *forward_deny;

  g_hash_table_iter_init (&iter, self->application_rows);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

  while (g_hash_table_iter_next (&iter, &row, NULL))
    {
      const char *name;

      if (adw_switch_row_get_active (ADW_SWITCH_ROW (row)))
        continue;

      name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
      g_variant_builder_add (&builder, "s", name);
    }

  forward_deny = g_variant_builder_end (&builder);
  settings = valent_device_preferences_group_get_settings (group);
  g_settings_set_value (settings, "forward-deny", forward_deny);
}

static void
add_application (ValentNotificationPreferences *self,
                 GVariant                      *app,
                 gboolean                       enabled)
{
  GtkWidget *row;
  GtkWidget *image;
  const char *title;
  g_autoptr (GVariant) icon_v = NULL;
  g_autoptr (GIcon) icon = NULL;

  if (!g_variant_lookup (app, "name", "&s", &title))
    return;

  row = g_object_new (ADW_TYPE_SWITCH_ROW,
                      "activatable", TRUE,
                      "selectable",  TRUE,
                      "title",       title,
                      "active",      enabled,
                      NULL);
  g_signal_connect_object (row,
                           "notify::active",
                           G_CALLBACK (on_switch_toggled),
                           self, 0);

  /* App icon */
  if ((icon_v = g_variant_lookup_value (app, "icon", NULL)) != NULL)
    icon = g_icon_deserialize (icon_v);

  if (icon == NULL)
    icon = g_icon_new_for_string ("application-x-executable", NULL);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "gicon",     icon,
                        "icon-size", GTK_ICON_SIZE_LARGE,
                        NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  g_hash_table_add (self->application_rows, row);
  gtk_list_box_insert (self->application_list, row, -1);
}

static void
populate_applications (ValentNotificationPreferences *self)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  GVariant *known;
  g_auto (GStrv) deny = NULL;
  GVariantIter iter;
  const char *key;
  GVariant *value;

  settings = valent_device_preferences_group_get_settings (group);
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

/*
 * GAction
 */
static void
applications_action (GtkWidget  *widget,
                     const char *action,
                     GVariant   *parameter)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (widget);
  GtkRoot *window = NULL;

  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));

  if ((window = gtk_widget_get_root (widget)) == NULL)
    return;

  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (window),
                                       self->application_page);
}

static void
reset_action (GtkWidget  *widget,
              const char *action,
              GVariant   *parameter)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (widget);
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (group);
  GSettings *settings;
  GHashTableIter iter;
  gpointer row_switch;

  g_assert (VALENT_IS_NOTIFICATION_PREFERENCES (self));
  g_assert (VALENT_IS_DEVICE_PREFERENCES_GROUP (self));

  g_hash_table_iter_init (&iter, self->application_rows);

  while (g_hash_table_iter_next (&iter, NULL, &row_switch))
    gtk_switch_set_active (GTK_SWITCH (row_switch), TRUE);

  settings = valent_device_preferences_group_get_settings (group);
  g_settings_reset (settings, "applications");
}

/*
 * GObject
 */
static void
valent_notification_preferences_constructed (GObject *object)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;

  settings = valent_device_preferences_group_get_settings (group);
  g_settings_bind (settings,                    "forward-notifications",
                   self->forward_notifications, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,                    "forward-when-active",
                   self->forward_when_active,   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  gtk_list_box_set_filter_func (self->application_list,
                                application_list_filter,
                                self, NULL);
  gtk_list_box_set_sort_func (self->application_list,
                              application_list_sort,
                              self, NULL);
  populate_applications (self);

  G_OBJECT_CLASS (valent_notification_preferences_parent_class)->constructed (object);
}

static void
valent_notification_preferences_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_NOTIFICATION_PREFERENCES);

  G_OBJECT_CLASS (valent_notification_preferences_parent_class)->dispose (object);
}

static void
valent_notification_preferences_finalize (GObject *object)
{
  ValentNotificationPreferences *self = VALENT_NOTIFICATION_PREFERENCES (object);

  g_clear_pointer (&self->application_filter, g_free);
  g_clear_pointer (&self->application_rows, g_hash_table_unref);

  G_OBJECT_CLASS (valent_notification_preferences_parent_class)->finalize (object);
}

static void
valent_notification_preferences_class_init (ValentNotificationPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_notification_preferences_constructed;
  object_class->dispose = valent_notification_preferences_dispose;
  object_class->finalize = valent_notification_preferences_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/notification/valent-notification-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_notifications);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, forward_when_active);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_page);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_title_stack);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_filter_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_filter_button);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_list);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationPreferences, application_row);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_started);
  gtk_widget_class_bind_template_callback (widget_class, on_search_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_stop_search);

  gtk_widget_class_install_action (widget_class, "preferences.applications", NULL, applications_action);
  gtk_widget_class_install_action (widget_class, "preferences.reset", NULL, reset_action);
}

static void
valent_notification_preferences_init (ValentNotificationPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_search_entry_set_key_capture_widget (self->application_filter_entry,
                                           GTK_WIDGET (self->application_page));

  self->application_rows = g_hash_table_new (NULL, NULL);
}

