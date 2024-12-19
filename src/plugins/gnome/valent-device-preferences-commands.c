// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <libportal/portal.h>
#include <valent.h>

#include "valent-device-preferences-commands-editor.h"

#include "valent-device-preferences-commands.h"


struct _ValentRuncommandPreferences
{
  ValentDevicePreferencesGroup  parent_instance;

  GtkWindow                    *editor;

  /* template */
  AdwExpanderRow               *command_list_row;
  GtkListBox                   *command_list;
};

G_DEFINE_FINAL_TYPE (ValentRuncommandPreferences, valent_runcommand_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


/*
 * Rows
 */
static void
valent_runcommand_preferences_populate (ValentRuncommandPreferences *self)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  GtkWidget *child;
  g_autoptr (GVariant) commands = NULL;
  GVariantIter iter;
  char *uuid;
  GVariant *item;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  for (child = gtk_widget_get_first_child (GTK_WIDGET (self->command_list));
       child != NULL;)
    {
      GtkWidget *current_child = child;
      child = gtk_widget_get_next_sibling (current_child);
      gtk_list_box_remove (self->command_list, current_child);
    }

  settings = valent_device_preferences_group_get_settings (group);
  commands = g_settings_get_value (settings, "commands");

  g_variant_iter_init (&iter, commands);

  while (g_variant_iter_next (&iter, "{sv}", &uuid, &item))
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
                              "action-name",   "runcommand.edit",
                              NULL);

          icon = g_object_new (GTK_TYPE_IMAGE,
                               "icon-name",    "document-edit-symbolic",
                               "tooltip-text", _("Edit Command"),
                               NULL);
          gtk_widget_add_css_class (icon, "dim-label");
          adw_action_row_add_suffix (ADW_ACTION_ROW (row), icon);

          /* a11y: an activatable widget would change the label and description,
           * but a button is unnecessary and the label should be unchanged. */
          gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                          GTK_ACCESSIBLE_RELATION_DESCRIBED_BY,
                                            icon, NULL,
                                          -1);

          gtk_list_box_append (self->command_list, row);
        }

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

static inline GtkListBox *
_adw_expander_row_get_list (AdwExpanderRow *row)
{
  GtkWidget *box;
  GtkWidget *child;

  /* First child is a GtkBox */
  box = gtk_widget_get_first_child (GTK_WIDGET (row));

  /* The GtkBox contains the primary AdwActionRow and a GtkRevealer */
  for (child = gtk_widget_get_first_child (box);
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (GTK_IS_REVEALER (child))
        break;
    }

  /* The GtkRevealer's child is the GtkListBox */
  if (GTK_IS_REVEALER (child))
    return GTK_LIST_BOX (gtk_revealer_get_child (GTK_REVEALER (child)));

  return NULL;
}

/*
 * GAction
 */
static void
on_command_changed (ValentRuncommandEditor      *editor,
                    GParamSpec                  *pspec,
                    ValentRuncommandPreferences *self)
{
  const char *uuid;
  GVariant *command;

  g_assert (VALENT_IS_RUNCOMMAND_EDITOR (editor));
  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  uuid = valent_runcommand_editor_get_uuid (editor);
  command = valent_runcommand_editor_get_command (editor);

  if (command != NULL)
    gtk_widget_activate_action (GTK_WIDGET (self), "runcommand.save", "s", uuid);
  else
    gtk_widget_activate_action (GTK_WIDGET (self), "runcommand.remove", "s", uuid);

  gtk_window_destroy (GTK_WINDOW (editor));
}

static void
runcommand_add_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *parameter)
{
  g_autofree char *uuid = NULL;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (widget));

  uuid = g_uuid_string_random ();
  gtk_widget_activate_action (widget, "runcommand.edit", "s", uuid);
}

static void
runcommand_edit_action (GtkWidget  *widget,
                        const char *action_name,
                        GVariant   *parameter)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (widget);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  const char *uuid = NULL;
  GSettings *settings;
  GtkRoot *window;
  g_autoptr (GVariant) commands = NULL;
  g_autoptr (GVariant) command = NULL;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  uuid = g_variant_get_string (parameter, NULL);
  settings = valent_device_preferences_group_get_settings (group);
  commands = g_settings_get_value (settings, "commands");
  g_variant_lookup (commands, uuid, "@a{sv}", &command);

  window = gtk_widget_get_root (GTK_WIDGET (self));
  self->editor = g_object_new (VALENT_TYPE_RUNCOMMAND_EDITOR,
                               "uuid",          uuid,
                               "command",       command,
                               "modal",         (window != NULL),
                               "transient-for", window,
                               NULL);
  g_object_add_weak_pointer (G_OBJECT (self->editor),
                             (gpointer) &self->editor);
  g_signal_connect_object (self->editor,
                           "notify::command",
                           G_CALLBACK (on_command_changed),
                           self, 0);

  gtk_window_present (self->editor);
}

static void
runcommand_save_action (GtkWidget  *widget,
                        const char *action_name,
                        GVariant   *parameter)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (widget);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  g_autoptr (GVariant) commands = NULL;
  GVariant *command;
  const char *uuid;
  GVariantDict dict;
  GVariant *value;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  settings = valent_device_preferences_group_get_settings (group);
  commands = g_settings_get_value (settings, "commands");

  uuid = valent_runcommand_editor_get_uuid (VALENT_RUNCOMMAND_EDITOR (self->editor));
  command = valent_runcommand_editor_get_command (VALENT_RUNCOMMAND_EDITOR (self->editor));

  g_variant_dict_init (&dict, commands);
  g_variant_dict_insert_value (&dict, uuid, command);
  value = g_variant_dict_end (&dict);

  g_settings_set_value (settings, "commands", value);
}

static void
runcommand_remove_action (GtkWidget  *widget,
                          const char *action_name,
                          GVariant   *parameter)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (widget);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  g_autoptr (GVariant) commands = NULL;
  GVariantDict dict;
  GVariant *value;
  const char *uuid = NULL;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  uuid = g_variant_get_string (parameter, NULL);
  settings = valent_device_preferences_group_get_settings (group);
  commands = g_settings_get_value (settings, "commands");

  g_variant_dict_init (&dict, commands);
  g_variant_dict_remove (&dict, uuid);
  value = g_variant_dict_end (&dict);

  g_settings_set_value (settings, "commands", value);
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

  G_OBJECT_CLASS (valent_runcommand_preferences_parent_class)->constructed (object);

  gtk_list_box_set_sort_func (self->command_list, sort_commands, self, NULL);

  /* Setup GSettings */
  settings = valent_device_preferences_group_get_settings (group);
  g_signal_connect_object (settings,
                           "changed::commands",
                           G_CALLBACK (valent_runcommand_preferences_populate),
                           self,
                           G_CONNECT_SWAPPED);
  valent_runcommand_preferences_populate (self);
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

  g_clear_pointer (&self->editor, gtk_window_destroy);

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

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-device-preferences-commands.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, command_list_row);

  gtk_widget_class_install_action (widget_class, "runcommand.add", NULL, runcommand_add_action);
  gtk_widget_class_install_action (widget_class, "runcommand.edit", "s", runcommand_edit_action);
  gtk_widget_class_install_action (widget_class, "runcommand.remove", "s", runcommand_remove_action);
  gtk_widget_class_install_action (widget_class, "runcommand.save", "s", runcommand_save_action);
}

static void
valent_runcommand_preferences_init (ValentRuncommandPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->command_list = _adw_expander_row_get_list (self->command_list_row);
}

