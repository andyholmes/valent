// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-runcommand-editor.h"
#include "valent-runcommand-preferences.h"
#include "valent-runcommand-utils.h"


struct _ValentRuncommandPreferences
{
  AdwPreferencesPage   parent_instance;

  GSettings           *settings;
  char                *plugin_context;

  GtkDialog           *command_dialog;

  /* Template widgets */
  AdwPreferencesGroup *command_group;
  GtkListBox          *command_list;
  char                *command_uuid;
  GtkWidget           *command_add;

  AdwPreferencesGroup *restrictions_group;
  GtkSwitch           *isolate_subprocesses;

  gint                 commands_changed_id;
};

/* Interfaces */
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

static void valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentRuncommandPreferences, valent_runcommand_preferences, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_PLUGIN_PREFERENCES, valent_plugin_preferences_iface_init))

enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  N_PROPERTIES
};


/*
 * GSettings functions
 */
static void
edit_command_response (GtkDialog                   *dialog,
                       gint                         response_id,
                       ValentRuncommandPreferences *self)
{
  ValentRuncommandEditor *editor = VALENT_RUNCOMMAND_EDITOR (dialog);
  const char *command;
  const char *name;

  g_assert (VALENT_IS_RUNCOMMAND_EDITOR (editor));

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      command = valent_runcommand_editor_get_command (editor);
      name = valent_runcommand_editor_get_name (editor);
      save_command (self, self->command_uuid, name, command);
    }

  g_clear_pointer (&self->command_uuid, g_free);
  valent_runcommand_editor_set_name (editor, "");
  valent_runcommand_editor_set_command (editor, "");

  gtk_window_close (GTK_WINDOW (dialog));
}

static void
edit_command (ValentRuncommandPreferences *self,
              const char                  *uuid,
              const char                  *name,
              const char                  *command)
{
  ValentRuncommandEditor *editor = VALENT_RUNCOMMAND_EDITOR (self->command_dialog);

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

      editor = VALENT_RUNCOMMAND_EDITOR (self->command_dialog);
    }

  self->command_uuid = g_strdup (uuid);
  valent_runcommand_editor_set_name (editor, name);
  valent_runcommand_editor_set_command (editor, command);

  gtk_window_present_with_time (GTK_WINDOW (self->command_dialog),
                                GDK_CURRENT_TIME);
}

static void
remove_command (ValentRuncommandPreferences *self,
                const char                  *uuid)
{
  g_autoptr (GVariant) cmds_var = NULL;
  g_autoptr (GVariantDict) cmds_dict = NULL;
  GVariant *cmds_new;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  /* Get the current commands GVariant */
  cmds_var = g_settings_get_value (self->settings, "commands");

  /* Create the new commands GVariant (floating) */
  cmds_dict = g_variant_dict_new (cmds_var);
  g_variant_dict_remove (cmds_dict, uuid);
  cmds_new = g_variant_dict_end (cmds_dict);

  /* Save the new commands GVariant */
  g_settings_set_value (self->settings, "commands", cmds_new);

  g_clear_pointer (&self->command_uuid, g_free);
}

static void
save_command (ValentRuncommandPreferences *self,
              const char                  *uuid,
              const char                  *name,
              const char                  *command)
{
  g_autoptr (GVariant) cmds_var = NULL;
  g_autoptr (GVariantDict) cmds_dict = NULL;
  GVariant *commandv;
  g_autoptr (GVariantDict) commandd = NULL;
  GVariant *cmds_new;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  /* Get the current commands GVariant */
  cmds_var = g_settings_get_value (self->settings, "commands");

  /* Create the new command GVariant (floating) */
  commandd = g_variant_dict_new (NULL);
  g_variant_dict_insert (commandd, "name", "s", name);
  g_variant_dict_insert (commandd, "command", "s", command);
  commandv = g_variant_dict_end (commandd);

  /* Create the new commands GVariant (floating) */
  cmds_dict = g_variant_dict_new (cmds_var);
  g_variant_dict_insert_value (cmds_dict, uuid, commandv);
  cmds_new = g_variant_dict_end (cmds_dict);

  /* Save the new commands GVariant */
  g_settings_set_value (self->settings, "commands", cmds_new);

  g_clear_pointer (&self->command_uuid, g_free);
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
  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  self->command_uuid = g_uuid_string_random ();
  edit_command (self, self->command_uuid, "", "");
}

static void
on_edit_command (GtkButton *button,
                 gpointer   user_data)
{
  ValentRuncommandPreferences *self = user_data;
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
on_remove_command (GtkButton *button,
                   gpointer   user_data)
{
  ValentRuncommandPreferences *self = user_data;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  if (self->command_uuid == NULL)
    {
      GtkWidget *row;
      const char *uuid;

      row = gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_LIST_BOX_ROW);
      uuid = gtk_widget_get_name (GTK_WIDGET (row));
      self->command_uuid = g_strdup (uuid);
    }

  remove_command (self, self->command_uuid);
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
                      "name",             uuid,
                      "title",            name,
                      "subtitle",         command,
                      "activatable",      FALSE,
                      NULL);

  /* Buttons */
  buttons = g_object_new (GTK_TYPE_BOX,
                          "orientation", GTK_ORIENTATION_HORIZONTAL,
                          "spacing",     6,
                          "valign",      GTK_ALIGN_CENTER,
                          NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), buttons);

  edit = g_object_new (GTK_TYPE_BUTTON,
                       "icon-name", "document-edit-symbolic",
                       NULL);
  gtk_box_insert_child_after (GTK_BOX (buttons), edit, NULL);
  g_signal_connect (G_OBJECT (edit),
                    "clicked",
                    G_CALLBACK (on_edit_command),
                    self);

  delete = g_object_new (GTK_TYPE_BUTTON,
                         "icon-name", "edit-delete-symbolic",
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
  GtkWidget *child;
  g_autoptr (GVariant) cmds_var = NULL;
  GVariantIter iter;
  char *uuid;
  GVariant *cmd;

  g_assert (VALENT_IS_RUNCOMMAND_PREFERENCES (self));

  /* Clear list */
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->command_list))))
    gtk_list_box_remove (self->command_list, child);

  /* Get stored value */
  cmds_var = g_settings_get_value (self->settings, "commands");

  /* Iterate commands */
  g_variant_iter_init (&iter, cmds_var);

  while (g_variant_iter_next (&iter, "{sv}", &uuid, &cmd))
    {
      g_autoptr (GVariantDict) cmd_dict = NULL;
      g_autofree char *name = NULL;
      g_autofree char *command = NULL;

      cmd_dict = g_variant_dict_new (cmd);
      g_variant_dict_lookup (cmd_dict, "name", "s", &name);
      g_variant_dict_lookup (cmd_dict, "command", "s", &command);

      if (name && command)
        add_command_row (self, uuid, name, command);

      // must free data for ourselves
      g_free (uuid);
      g_variant_unref (cmd);
    }
}

static gint
sort_commands  (GtkListBoxRow *row1,
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
 * Options
 */
static void
on_isolate_subprocesses_response (GtkDialog                   *dialog,
                                  int                          response_id,
                                  ValentRuncommandPreferences *self)
{
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      gtk_switch_set_state (self->isolate_subprocesses, FALSE);
      gtk_switch_set_active (self->isolate_subprocesses, FALSE);
    }
  else
    {
      gtk_switch_set_state (self->isolate_subprocesses, TRUE);
      gtk_switch_set_active (self->isolate_subprocesses, TRUE);
    }

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static gboolean
on_isolate_subprocesses_changed (GtkSwitch                   *sw,
                                 gboolean                     state,
                                 ValentRuncommandPreferences *self)
{
  GtkRoot *root;
  GtkDialog *dialog;
  GtkWidget *button;

  if (state == TRUE)
    return FALSE;

  root = gtk_widget_get_root (GTK_WIDGET (self));
  dialog = g_object_new (GTK_TYPE_MESSAGE_DIALOG,
                         "text",           _("Run Unrestricted?"),
                         "secondary-text", _("Commands will be run on the host "
                                             "system without restriction."),
                         "message-type",   GTK_MESSAGE_WARNING,
                         "modal",          TRUE,
                         "transient-for",  root,
                         NULL);

  gtk_dialog_add_button (dialog, _("Cancel"), GTK_RESPONSE_CANCEL);
  button = gtk_button_new_with_label (_("I Understand"));
  gtk_style_context_add_class (gtk_widget_get_style_context (button),
                               "destructive-action");
  gtk_dialog_add_action_widget (dialog, button, GTK_RESPONSE_ACCEPT);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_isolate_subprocesses_response),
                    self);

  gtk_window_present_with_time (GTK_WINDOW (dialog), GDK_CURRENT_TIME);

  return TRUE;
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
valent_runcommand_preferences_constructed (GObject *object)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (object);

  /* Setup GSettings */
  self->settings = valent_device_plugin_new_settings (self->plugin_context,
                                                      "runcommand");

  self->commands_changed_id = g_signal_connect (self->settings,
                                                "changed::commands",
                                                G_CALLBACK (on_commands_changed),
                                                self);

  /* Populate list */
  gtk_list_box_set_sort_func (self->command_list, sort_commands, self, NULL);
  populate_commands (self);

  /* Options */
  if (!valent_runcommand_can_spawn_host ())
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->isolate_subprocesses), FALSE);
      gtk_switch_set_active (self->isolate_subprocesses, TRUE);
    }
  else if (!valent_runcommand_can_spawn_sandbox ())
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->isolate_subprocesses), FALSE);
      gtk_switch_set_active (self->isolate_subprocesses, FALSE);
    }
  else
    {
      gtk_widget_set_sensitive (GTK_WIDGET (self->isolate_subprocesses), TRUE);
      g_settings_bind (self->settings,
                       "isolate-subprocesses",
                       self->isolate_subprocesses,
                       "active",
                       G_SETTINGS_BIND_DEFAULT);
    }

  G_OBJECT_CLASS (valent_runcommand_preferences_parent_class)->constructed (object);
}

static void
valent_runcommand_preferences_finalize (GObject *object)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (object);

  if (self->command_dialog)
    gtk_window_destroy (GTK_WINDOW (self->command_dialog));

  g_clear_pointer (&self->plugin_context, g_free);
  g_clear_object (&self->settings);
  //g_signal_handler_disconnect (self->settings, self->commands_changed_id);

  G_OBJECT_CLASS (valent_runcommand_preferences_parent_class)->finalize (object);
}

static void
valent_runcommand_preferences_get_property (GObject    *object,
                                            guint       prop_id,
                                            GValue     *value,
                                            GParamSpec *pspec)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (object);

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
valent_runcommand_preferences_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  ValentRuncommandPreferences *self = VALENT_RUNCOMMAND_PREFERENCES (object);

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
valent_runcommand_preferences_class_init (ValentRuncommandPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_runcommand_preferences_constructed;
  object_class->finalize = valent_runcommand_preferences_finalize;
  object_class->get_property = valent_runcommand_preferences_get_property;
  object_class->set_property = valent_runcommand_preferences_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/runcommand/valent-runcommand-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, command_group);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, command_list);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, command_add);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, restrictions_group);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandPreferences, isolate_subprocesses);

  gtk_widget_class_bind_template_callback (widget_class, on_add_command);
  gtk_widget_class_bind_template_callback (widget_class, on_isolate_subprocesses_changed);

  g_object_class_override_property (object_class,
                                    PROP_PLUGIN_CONTEXT,
                                    "plugin-context");
}

static void
valent_runcommand_preferences_init (ValentRuncommandPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

