// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-share-preferences.h"


struct _ValentSharePreferences
{
  ValentDevicePreferencesPage  parent_instance;

  /* template */
  AdwPreferencesGroup         *download_group;
  GtkLabel                    *download_folder_label;
};

G_DEFINE_FINAL_TYPE (ValentSharePreferences, valent_share_preferences, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


/*
 * Download Folder
 */
static gboolean
on_download_folder_changed (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  const char *label;
  g_autofree char *basename = NULL;
  g_autofree char *result = NULL;

  label = g_variant_get_string (variant, NULL);
  basename = g_path_get_basename (label);
  result = g_strdup_printf ("…/%s", basename);

  g_value_set_string (value, result);

  return TRUE;
}

static void
on_download_folder_response (GtkNativeDialog             *dialog,
                             int                          response_id,
                             ValentDevicePreferencesPage *page)
{
  g_autoptr (GFile) file = NULL;

  g_assert (VALENT_IS_DEVICE_PREFERENCES_PAGE (page));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GSettings *settings;
      const char *path;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      path = g_file_peek_path (file);
      settings = valent_device_preferences_page_get_settings (page);
      g_settings_set_string (settings, "download-folder", path);
    }

  gtk_native_dialog_destroy (dialog);
}

/*
 * GActions
 */
static void
select_download_folder_action (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *parameter)
{
  ValentSharePreferences *self = VALENT_SHARE_PREFERENCES (widget);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;
  GtkNativeDialog *dialog;
  g_autofree char *path = NULL;

  g_assert (VALENT_IS_SHARE_PREFERENCES (self));

  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_NATIVE,
                         "action",        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                         "title",         _("Select download folder"),
                         "accept-label",  _("Open"),
                         "cancel-label",  _("Cancel"),
                         "modal",         TRUE,
                         "transient-for", gtk_widget_get_root (widget),
                         NULL);

  settings = valent_device_preferences_page_get_settings (page);
  path = g_settings_get_string (settings, "download-folder");

  if (strlen (path) > 0)
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_path (path);
      gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
                                           file, NULL);
    }

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_download_folder_response),
                    self);

  gtk_native_dialog_show (dialog);
}

/*
 * GObject
 */
static void
valent_share_preferences_constructed (GObject *object)
{
  ValentSharePreferences *self = VALENT_SHARE_PREFERENCES (object);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  g_autofree char *download_folder = NULL;
  GSettings *settings;

  /* Setup GSettings */
  settings = valent_device_preferences_page_get_settings (page);

  download_folder = g_settings_get_string (settings, "download-folder");

  if (download_folder == NULL || *download_folder == '\0')
    {
      const char *user_download = NULL;

      user_download = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
      valent_set_string (&download_folder, user_download);
      g_settings_set_string (settings, "download-folder", download_folder);
    }

  g_settings_bind_with_mapping (settings,                    "download-folder",
                                self->download_folder_label, "label",
                                G_SETTINGS_BIND_GET,
                                on_download_folder_changed,
                                NULL,
                                NULL, NULL);

  G_OBJECT_CLASS (valent_share_preferences_parent_class)->constructed (object);
}

static void
valent_share_preferences_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_SHARE_PREFERENCES);

  G_OBJECT_CLASS (valent_share_preferences_parent_class)->dispose (object);
}

static void
valent_share_preferences_class_init (ValentSharePreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_share_preferences_constructed;
  object_class->dispose = valent_share_preferences_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/share/valent-share-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentSharePreferences, download_group);
  gtk_widget_class_bind_template_child (widget_class, ValentSharePreferences, download_folder_label);
  gtk_widget_class_install_action (widget_class, "preferences.select-download-folder", NULL, select_download_folder_action);
}

static void
valent_share_preferences_init (ValentSharePreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

