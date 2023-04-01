// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <libportal/portal.h>
#include <valent.h>

#include "valent-runcommand-editor.h"
#include "valent-runcommand-preferences.h"
#include "valent-runcommand-utils.h"


struct _ValentRuncommandPreferences
{
  ValentDevicePreferencesGroup  parent_instance;

  GtkWindow                    *command_dialog;

  /* template */
  GtkListBox                   *command_list;
  GtkWidget                    *command_add;
};

G_DEFINE_FINAL_TYPE (ValentRuncommandPreferences, valent_runcommand_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


static void edit_command      (ValentRuncommandPreferences *self,
                               const char                  *uuid,
                               const char                  *name,
                               const char                  *command);
static void remove_command    (ValentRuncommandPreferences *self,
                               const char                  *uuid);
static void populate_commands (ValentRuncommandPreferences *self);
static void save_command      (ValentRuncommandPreferences *self,
                               const char                  *uuid,
                               const char                  *name,
                               const char                  *command);


/*
 * GSettings functions
 */
static void
edit_command_response (GtkDialog                   *dialog,
                       int                          response_id,
                       ValentRuncommandPreferences *self)
{
  ValentRuncommandEditor *editor = VALENT_RUNCOMMAND_EDITOR (dialog);

  g_assert (VALENT_IS_RUNCOMMAND_EDITOR (editor));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      const char *command;
      const char *name;
      const char *uuid;

      command = valent_runcommand_editor_get_command (editor);
      name = valent_runcommand_editor_get_name (editor);
      uuid = valent_runcommand_editor_get_uuid (editor);
      save_command (self, uuid, name, command);
    }

  valent_runcommand_editor_clear (editor);
  gtk_window_close (GTK_WINDOW (dialog));
}

static void
edit_command (ValentRuncommandPreferences *self,
              const char                  *uuid,
              const char                  *name,
              const char                  *command)
{
  ValentRuncommandEditor *editor;

  if (self->command_dialog == NULL)
    {
      GtkRoot *window;

      window = gtk_widget_get_root (GTK_WIDGET (self));
      self->command_dialog = g_object_new (VALENT_TYPE_RUNCOMMAND_EDITOR,
                                           "use-header-bar", TRUE,
                                           "modal",          (window != NULL),
                                           "transient-for",  window,
                                           NULL);

      g_signal_connect (self->command_dialog,
                        "response",
                        G_CALLBACK (edit_command_response),
                        self);
      g_object_add_weak_pointer (G_OBJECT (self->command_dialog),
                                 (gpointer) &self->command_dialog);
    }


  editor = VALENT_RUNCOMMAND_EDITOR (self->command_dialog);
  valent_runcommand_editor_set_uuid (editor, uuid);
  valent_runcommand_editor_set_name (editor, name);
  valent_runcommand_editor_set_command (editor, command);

  gtk_window_present_with_time (self->command_dialog, GDK_CURRENT_TIME);
}

static void
remove_command (ValentRuncommandPreferences *self,
                const char                  *uuid)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  g_autoptr (GVariant) commands = NULL;
  GVariantDict dict;
  GVariant *value;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  settings = valent_device_preferences_group_get_settings (group);
  commands = g_settings_get_value (settings, "commands");

  g_variant_dict_init (&dict, commands);
  g_variant_dict_remove (&dict, uuid);
  value = g_variant_dict_end (&dict);

  g_settings_set_value (settings, "commands", value);
}

static void
save_command (ValentRuncommandPreferences *self,
              const char                  *uuid,
              const char                  *name,
              const char                  *command)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  g_autoptr (GVariant) commands = NULL;
  GVariantDict dict;
  GVariant *item;
  GVariant *value;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  settings = valent_device_preferences_group_get_settings (group);
  commands = g_settings_get_value (settings, "commands");

  item = g_variant_new_parsed ("{'name': <%s>, 'command': <%s>}",
                               name,
                               command);

  g_variant_dict_init (&dict, commands);
  g_variant_dict_insert_value (&dict, uuid, item);
  value = g_variant_dict_end (&dict);

  g_settings_set_value (settings, "commands", value);
}

static void
on_commands_changed (GSettings                   *settings,
                     const char                  *key,
                     ValentRuncommandPreferences *self)
{
  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL);
  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  populate_commands (self);
}

/*
 * UI Callbacks
 */
static void
on_add_command (GtkListBox                  *box,
                GtkListBoxRow               *row,
                ValentRuncommandPreferences *self)
{
  g_autofree char *uuid = NULL;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  uuid = g_uuid_string_random ();
  edit_command (self, uuid, "", "");
}

static void
on_edit_command (GtkButton                   *button,
                 ValentRuncommandPreferences *self)
{
  GtkWidget *row;
  const char *name;
  const char *command;
  const char *uuid;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  uuid = gtk_widget_get_name (row);
  name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  command = adw_action_row_get_subtitle (ADW_ACTION_ROW (row));

  edit_command (self, uuid, name, command);
}

static void
on_remove_command (GtkButton                   *button,
                   ValentRuncommandPreferences *self)
{
  GtkWidget *row;
  const char *uuid;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
  uuid = gtk_widget_get_name (GTK_WIDGET (row));
  remove_command (self, uuid);
}

/*
 * Rows
 */
static void
add_command_row (ValentRuncommandPreferences *self,
                 const char                  *uuid,
                 const char                  *name,
                 const char                  *command)
{
  GtkWidget *row;
  GtkWidget *edit;
  GtkWidget *delete;
  GtkWidget *buttons;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  /* Row */
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "name",        uuid,
                      "title",       name,
                      "subtitle",    command,
                      "activatable", FALSE,
                      NULL);

  /* Buttons */
  buttons = g_object_new (GTK_TYPE_BOX,
                          "orientation", GTK_ORIENTATION_HORIZONTAL,
                          "spacing",     6,
                          "valign",      GTK_ALIGN_CENTER,
                          NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), buttons);

  edit = g_object_new (GTK_TYPE_BUTTON,
                       "icon-name",    "document-edit-symbolic",
                       "tooltip-text", _("Edit"),
                       NULL);
  gtk_box_insert_child_after (GTK_BOX (buttons), edit, NULL);
  g_signal_connect (G_OBJECT (edit),
                    "clicked",
                    G_CALLBACK (on_edit_command),
                    self);

  delete = g_object_new (GTK_TYPE_BUTTON,
                         "icon-name",    "edit-delete-symbolic",
                         "tooltip-text", _("Remove"),
                         NULL);
  gtk_box_insert_child_after (GTK_BOX (buttons), delete, edit);
  g_signal_connect (G_OBJECT (delete),
                    "clicked",
                    G_CALLBACK (on_remove_command),
                    self);

  gtk_list_box_insert (self->command_list, row, -1);
}

static void
populate_commands (ValentRuncommandPreferences *self)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GtkWidget *child;
  GSettings *settings;
  g_autoptr (GVariant) commands = NULL;
  GVariantIter iter;
  char *uuid;
  GVariant *item;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->command_list))))
    gtk_list_box_remove (self->command_list, child);

  settings = valent_device_preferences_group_get_settings (group);
  commands = g_settings_get_value (settings, "commands");

  g_variant_iter_init (&iter, commands);

  while (g_variant_iter_next (&iter, "{sv}", &uuid, &item))
    {
      const char *name = NULL;
      const char *command = NULL;

      if (g_variant_lookup (item, "name", "&s", &name) &&
          g_variant_lookup (item, "command", "&s", &command))
        add_command_row (self, uuid, name, command);

      g_clear_pointer (&uuid, g_free);
      g_clear_pointer (&item, g_variant_unref);
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
 * GObject
 */
static void
valent_runcommand_preferences_constructed (GObject *object)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (object);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;

  settings = valent_device_preferences_group_get_settings (group);
  g_signal_connect_object (settings,
                           "changed::commands",
                           G_CALLBACK (on_commands_changed),
                           self, 0);

  /* Populate list */
  gtk_list_box_set_sort_func (self->command_list, sort_commands, self, NULL);
  populate_commands (self);

  G_OBJECT_CLASS (valent_runcommand_preferences_parent_class)->constructed (object);
}

static void
valent_runcommand_preferences_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_RUNCOMMAND_PREFERENCES);

  G_OBJECT_CLASS (valent_runcommand_preferences_parent_class)->dispose (object);
}

static void
valent_runcommand_preferences_finalize (GObject *object)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (object);

  g_clear_pointer (&self->command_dialog, gtk_window_destroy);

  G_OBJECT_CLASS (valent_runcommand_preferences_parent_class)->finalize (object);
}

static void
valent_runcommand_preferences_class_init (ValentRuncommandPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_runcommand_preferences_constructed;
  object_class->dispose = valent_runcommand_preferences_dispose;
  object_class->finalize = valent_runcommand_preferences_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/runcommand/valent-runcommand-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, command_list);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, command_add);
  gtk_widget_class_bind_template_callback (widget_class, on_add_command);
}

static void
valent_runcommand_preferences_init (ValentRuncommandPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

