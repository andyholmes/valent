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
  ValentDevicePreferencesGroup  parent_instance;

  /* template */
  GtkLabel                     *download_folder_label;
};

G_DEFINE_FINAL_TYPE (ValentSharePreferences, valent_share_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


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
gtk_file_dialog_select_folder_cb (GtkFileDialog                *dialog,
                                  GAsyncResult                 *result,
                                  ValentDevicePreferencesGroup *group)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;
  GSettings *settings;
  const char *path;

  g_assert (VALENT_IS_DEVICE_PREFERENCES_GROUP (group));

  file = gtk_file_dialog_select_folder_finish (dialog, result, &error);

  if (file == NULL)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  path = g_file_peek_path (file);
  settings = valent_device_preferences_group_get_settings (group);
  g_settings_set_string (settings, "download-folder", path);
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
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  g_autoptr (GtkFileDialog) dialog = NULL;
  GSettings *settings;
  g_autofree char *path = NULL;

  g_assert (VALENT_IS_SHARE_PREFERENCES (self));

  dialog = g_object_new (GTK_TYPE_FILE_DIALOG,
                         "title",        _("Select download folder"),
                         "accept-label", _("Open"),
                         "modal",        TRUE,
                         NULL);

  settings = valent_device_preferences_group_get_settings (group);
  path = g_settings_get_string (settings, "download-folder");

  if (strlen (path) > 0)
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_path (path);
      gtk_file_dialog_set_initial_folder (dialog, file);
    }

  gtk_file_dialog_select_folder (dialog,
                                 GTK_WINDOW (gtk_widget_get_root (widget)),
                                 NULL,
                                 (GAsyncReadyCallback)gtk_file_dialog_select_folder_cb,
                                 self);
}

/*
 * GObject
 */
static void
valent_share_preferences_constructed (GObject *object)
{
  ValentSharePreferences *self = VALENT_SHARE_PREFERENCES (object);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  g_autofree char *download_folder = NULL;
  GSettings *settings;

  settings = valent_device_preferences_group_get_settings (group);
  download_folder = g_settings_get_string (settings, "download-folder");

  if (download_folder == NULL || *download_folder == '\0')
    {
      const char *user_download = NULL;

      user_download = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
      g_set_str (&download_folder, user_download);
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
  gtk_widget_class_bind_template_child (widget_class, ValentSharePreferences, download_folder_label);
  gtk_widget_class_install_action (widget_class, "preferences.select-download-folder", NULL, select_download_folder_action);
}

static void
valent_share_preferences_init (ValentSharePreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

