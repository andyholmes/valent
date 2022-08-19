// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-share-preferences.h"


struct _ValentSharePreferences
{
  AdwPreferencesPage   parent_instance;

  char                *device_id;
  PeasPluginInfo      *plugin_info;
  GSettings           *settings;

  /* Template widgets */
  AdwPreferencesGroup *download_group;
  GtkLabel            *download_folder_label;
};

/* Interfaces */
static void valent_device_preferences_page_iface_init (ValentDevicePreferencesPageInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentSharePreferences, valent_share_preferences, ADW_TYPE_PREFERENCES_PAGE,
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
on_download_folder_response (GtkNativeDialog        *dialog,
                             int                     response_id,
                             ValentSharePreferences *self)
{
  g_autoptr (GFile) file = NULL;

  g_assert (VALENT_IS_SHARE_PREFERENCES (self));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      const char *path;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      path = g_file_peek_path (file);
      g_settings_set_string (self->settings, "download-folder", path);
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

  path = g_settings_get_string (self->settings, "download-folder");

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
  g_autofree char *download_folder = NULL;

  /* Setup GSettings */
  self->settings = valent_device_plugin_create_settings (self->plugin_info,
                                                         self->device_id);

  download_folder = g_settings_get_string (self->settings, "download-folder");

  if (download_folder == NULL || *download_folder == '\0')
    {
      g_clear_pointer (&download_folder, g_free);
      download_folder = valent_data_get_directory (G_USER_DIRECTORY_DOWNLOAD);
      g_settings_set_string (self->settings, "download-folder", download_folder);
    }

  g_settings_bind_with_mapping (self->settings,              "download-folder",
                                self->download_folder_label, "label",
                                G_SETTINGS_BIND_GET,
                                on_download_folder_changed,
                                NULL,
                                NULL, NULL);

  G_OBJECT_CLASS (valent_share_preferences_parent_class)->constructed (object);
}

static void
valent_share_preferences_finalize (GObject *object)
{
  ValentSharePreferences *self = VALENT_SHARE_PREFERENCES (object);

  g_clear_pointer (&self->device_id, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_share_preferences_parent_class)->finalize (object);
}

static void
valent_share_preferences_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  ValentSharePreferences *self = VALENT_SHARE_PREFERENCES (object);

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
valent_share_preferences_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  ValentSharePreferences *self = VALENT_SHARE_PREFERENCES (object);

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
valent_share_preferences_class_init (ValentSharePreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_share_preferences_constructed;
  object_class->finalize = valent_share_preferences_finalize;
  object_class->get_property = valent_share_preferences_get_property;
  object_class->set_property = valent_share_preferences_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/share/valent-share-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentSharePreferences, download_group);
  gtk_widget_class_bind_template_child (widget_class, ValentSharePreferences, download_folder_label);
  gtk_widget_class_install_action (widget_class, "preferences.select-download-folder", NULL, select_download_folder_action);

  g_object_class_override_property (object_class, PROP_DEVICE_ID, "device-id");
  g_object_class_override_property (object_class, PROP_PLUGIN_INFO, "plugin-info");
}

static void
valent_share_preferences_init (ValentSharePreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

