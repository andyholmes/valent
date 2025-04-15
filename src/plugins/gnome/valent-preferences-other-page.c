// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences-dialog"

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-preferences-command-editor.h"

#include "valent-preferences-other-page.h"


struct _ValentPreferencesOtherPage
{
  ValentPreferencesPage  parent_instance;

  /* template */
  GtkListBox            *command_list;
  GtkLabel              *download_folder_label;
};

G_DEFINE_FINAL_TYPE (ValentPreferencesOtherPage, valent_preferences_other_page, VALENT_TYPE_PREFERENCES_PAGE)

/*
 * Commands
 */
static void
valent_runcommand_preferences_populate (ValentPreferencesOtherPage *self)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  GSettings *settings;
  GtkWidget *child;
  g_autoptr (GVariant) commands = NULL;
  GVariantIter iter;
  char *uuid;
  GVariant *item;

  g_assert (VALENT_IS_PREFERENCES_OTHER_PAGE (self));

  child = gtk_widget_get_first_child (GTK_WIDGET (self->command_list));
  while (child != NULL)
    {
      GtkWidget *current = child;

      child = gtk_widget_get_next_sibling (current);
      if (ADW_IS_ACTION_ROW (current))
        gtk_list_box_remove (self->command_list, current);
    }

  settings = valent_preferences_page_get_settings (page, "runcommand");
  commands = g_settings_get_value (settings, "commands");

  g_variant_iter_init (&iter, commands);
  while (g_variant_iter_loop (&iter, "{sv}", &uuid, &item))
    {
      const char *name = NULL;
      const char *command = NULL;

      if (g_variant_lookup (item, "name", "&s", &name) &&
          g_variant_lookup (item, "command", "&s", &command))
        {
          GtkWidget *row;
          GtkWidget *icon;

          row = g_object_new (ADW_TYPE_ACTION_ROW,
                              "name",          uuid,
                              "title",         name,
                              "subtitle",      command,
                              "activatable",   TRUE,
                              "selectable",    FALSE,
                              "action-target", g_variant_new_string (uuid),
                              "action-name",   "preferences.edit-command",
                              NULL);

          icon = g_object_new (GTK_TYPE_IMAGE,
                               "icon-name",    "document-edit-symbolic",
                               "tooltip-text", _("Edit Command"),
                               NULL);
          adw_action_row_add_suffix (ADW_ACTION_ROW (row), icon);

          /* a11y: an activatable widget would change the label and description,
           * but a button is unnecessary and the label should be unchanged. */
          gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                          GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
                                          icon, NULL,
                                          -1);

          gtk_list_box_append (self->command_list, row);
        }
    }
}

static int
sort_commands (GtkListBoxRow *row1,
               GtkListBoxRow *row2,
               gpointer       user_data)
{
  const char *title1;
  const char *title2;

  title1 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row1));
  title2 = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row2));

  return g_utf8_collate (title1, title2);
}

/*
 * GAction
 */
static void
on_command_changed (ValentPreferencesCommandEditor *editor,
                    GParamSpec                     *pspec,
                    ValentPreferencesOtherPage     *self)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  GSettings *settings;
  g_autoptr (GVariant) commands = NULL;
  const char *uuid;
  GVariant *command;
  GVariantDict dict;

  g_assert (VALENT_IS_PREFERENCES_COMMAND_EDITOR (editor));
  g_assert (VALENT_IS_PREFERENCES_OTHER_PAGE (self));

  settings = valent_preferences_page_get_settings (page, "runcommand");
  commands = g_settings_get_value (settings, "commands");

  uuid = valent_preferences_command_editor_get_uuid (editor);
  command = valent_preferences_command_editor_get_command (editor);
  if (command != NULL)
    {
      g_variant_dict_init (&dict, commands);
      g_variant_dict_insert_value (&dict, uuid, command);
      g_settings_set_value (settings, "commands", g_variant_dict_end (&dict));
    }
  else
    {
      g_variant_dict_init (&dict, commands);
      g_variant_dict_remove (&dict, uuid);
      g_settings_set_value (settings, "commands", g_variant_dict_end (&dict));
    }

  adw_dialog_close (ADW_DIALOG (editor));
}

static void
add_command_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  g_autofree char *uuid = NULL;

  g_assert (VALENT_IS_PREFERENCES_OTHER_PAGE (widget));

  uuid = g_uuid_string_random ();
  gtk_widget_activate_action (widget, "preferences.edit-command", "s", uuid);
}

static void
edit_command_action (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *parameter)
{
  ValentPreferencesOtherPage *self = VALENT_PREFERENCES_OTHER_PAGE (widget);
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  AdwDialog *editor = NULL;
  const char *uuid = NULL;
  GSettings *settings;
  g_autoptr (GVariant) commands = NULL;
  g_autoptr (GVariant) command = NULL;

  g_assert (VALENT_IS_PREFERENCES_OTHER_PAGE (self));

  uuid = g_variant_get_string (parameter, NULL);
  settings = valent_preferences_page_get_settings (page, "runcommand");
  commands = g_settings_get_value (settings, "commands");
  g_variant_lookup (commands, uuid, "@a{sv}", &command);

  editor = g_object_new (VALENT_TYPE_PREFERENCES_COMMAND_EDITOR,
                         "uuid",    uuid,
                         "command", command,
                         NULL);
  g_signal_connect_object (editor,
                           "notify::command",
                           G_CALLBACK (on_command_changed),
                           self,
                           G_CONNECT_DEFAULT);
  adw_dialog_present (editor, widget);
}

/*
 * Share
 */
static gboolean
on_download_folder_changed (GValue   *value,
                            GVariant *variant,
                            gpointer  user_data)
{
  const char *label;

  label = g_variant_get_string (variant, NULL);
  g_value_take_string (value, g_path_get_basename (label));

  return TRUE;
}

static void
gtk_file_dialog_select_folder_cb (GtkFileDialog         *dialog,
                                  GAsyncResult          *result,
                                  ValentPreferencesPage *page)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;
  GSettings *settings = NULL;

  g_assert (VALENT_IS_PREFERENCES_PAGE (page));

  file = gtk_file_dialog_select_folder_finish (dialog, result, &error);
  if (file == NULL)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  settings = valent_preferences_page_get_settings (page, "share");
  g_settings_set_string (settings, "download-folder", g_file_peek_path (file));
}

static void
select_download_folder_action (GtkWidget  *widget,
                               const char *action_name,
                               GVariant   *parameter)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (widget);
  g_autoptr (GtkFileDialog) dialog = NULL;
  GSettings *settings;
  g_autofree char *path = NULL;

  dialog = g_object_new (GTK_TYPE_FILE_DIALOG,
                         "title",        _("Select download folder"),
                         "accept-label", _("Open"),
                         "modal",        TRUE,
                         NULL);

  settings = valent_preferences_page_get_settings (page, "share");
  path = g_settings_get_string (settings, "download-folder");
  if (path != NULL && *path != '\0')
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_path (path);
      gtk_file_dialog_set_initial_folder (dialog, file);
    }

  gtk_file_dialog_select_folder (dialog,
                                 GTK_WINDOW (gtk_widget_get_root (widget)),
                                 NULL,
                                 (GAsyncReadyCallback)gtk_file_dialog_select_folder_cb,
                                 widget);
}

/*
 * ValentPreferencesPage
 */
static inline void
valent_preferences_other_page_bind_context (ValentPreferencesOtherPage *self)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  g_autofree char *download_folder = NULL;
  GSettings *settings = NULL;

  /* Commands
   */
  settings = valent_preferences_page_get_settings (page, "runcommand");
  g_signal_connect_object (settings,
                           "changed::commands",
                           G_CALLBACK (valent_runcommand_preferences_populate),
                           self,
                           G_CONNECT_SWAPPED);
  valent_runcommand_preferences_populate (self);

  /* Share
   */
  settings = valent_preferences_page_get_settings (page, "share");
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
}

/*
 * GObject
 */
static void
valent_preferences_other_page_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_PREFERENCES_OTHER_PAGE);

  G_OBJECT_CLASS (valent_preferences_other_page_parent_class)->dispose (object);
}

static void
valent_preferences_other_page_notify (GObject    *object,
                                     GParamSpec *pspec)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (object);
  ValentPreferencesOtherPage *self = VALENT_PREFERENCES_OTHER_PAGE (object);

  if (g_strcmp0 (pspec->name, "context") == 0)
    {
      if (valent_preferences_page_get_context (page) != NULL)
        valent_preferences_other_page_bind_context (self);
    }

  if (G_OBJECT_CLASS (valent_preferences_other_page_parent_class)->notify)
    G_OBJECT_CLASS (valent_preferences_other_page_parent_class)->notify (object,
                                                                          pspec);
}

static void
valent_preferences_other_page_class_init (ValentPreferencesOtherPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_preferences_other_page_dispose;
  object_class->notify = valent_preferences_other_page_notify;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-preferences-other-page.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesOtherPage, command_list);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesOtherPage, download_folder_label);
  gtk_widget_class_install_action (widget_class, "preferences.add-command", NULL, add_command_action);
  gtk_widget_class_install_action (widget_class, "preferences.edit-command", "s", edit_command_action);
  gtk_widget_class_install_action (widget_class, "preferences.select-download-folder", NULL, select_download_folder_action);
}

static void
valent_preferences_other_page_init (ValentPreferencesOtherPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
  gtk_list_box_set_sort_func (self->command_list, sort_commands, self, NULL);
}

