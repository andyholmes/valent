// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-editor"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-runcommand-editor.h"


struct _ValentRuncommandEditor
{
  GtkDialog  parent_instance;

  char      *uuid;

  /* template */
  GtkButton *cancel_button;
  GtkButton *save_button;
  GtkEntry  *command_entry;
  GtkEntry  *name_entry;
};

G_DEFINE_TYPE (ValentRuncommandEditor, valent_runcommand_editor, GTK_TYPE_DIALOG)


/*
 * GtkDialog
 */
static void
on_entry_changed (GtkEntry               *entry,
                  ValentRuncommandEditor *self)
{
  const char *command;
  const char *name;

  command = valent_runcommand_editor_get_command (self);
  name = valent_runcommand_editor_get_name (self);

  if ((name != NULL && *name != '\0') && (command != NULL && *command != '\0'))
    gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), TRUE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (self->save_button), FALSE);
}

/*
 * GObject
 */
static void
valent_runcommand_editor_finalize (GObject *object)
{
  ValentRuncommandEditor *self = VALENT_RUNCOMMAND_EDITOR (object);

  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (valent_runcommand_editor_parent_class)->finalize (object);
}

static void
valent_runcommand_editor_class_init (ValentRuncommandEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_runcommand_editor_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/runcommand/valent-runcommand-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, save_button);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, command_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, name_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);
}

static void
valent_runcommand_editor_init (ValentRuncommandEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->uuid = g_strdup ("");
}

/**
 * valent_runcommand_editor_get_command:
 * @editor: a #ValentRuncommandEditor
 *
 * Get the command-line entry text for @editor
 *
 * Returns: (not nullable) (transfer none): the command-line
 */
const char *
valent_runcommand_editor_get_command (ValentRuncommandEditor *editor)
{
  g_return_val_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor), NULL);

  return gtk_editable_get_text (GTK_EDITABLE (editor->command_entry));
}

/**
 * valent_runcommand_editor_set_command:
 * @editor: a #ValentRuncommandEditor
 * @command: a command-line
 *
 * Set the command-line entry text for @editor to @command.
 */
void
valent_runcommand_editor_set_command (ValentRuncommandEditor *editor,
                                      const char             *command)
{
  g_return_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor));

  gtk_editable_set_text (GTK_EDITABLE (editor->command_entry), command);
}

/**
 * valent_runcommand_editor_get_name:
 * @editor: a #ValentRuncommandEditor
 *
 * Get the command name entry text for @editor
 *
 * Returns: (not nullable) (transfer none): the command name
 */
const char *
valent_runcommand_editor_get_name (ValentRuncommandEditor *editor)
{
  g_return_val_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor), NULL);

  return gtk_editable_get_text (GTK_EDITABLE (editor->name_entry));
}

/**
 * valent_runcommand_editor_set_name:
 * @editor: a #ValentRuncommandEditor
 * @name: a command name
 *
 * Set the command name entry text for @editor to @command.
 */
void
valent_runcommand_editor_set_name (ValentRuncommandEditor *editor,
                                   const char             *name)
{
  g_return_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor));

  gtk_editable_set_text (GTK_EDITABLE (editor->name_entry), name);
}

/**
 * valent_runcommand_editor_get_uuid:
 * @editor: a #ValentRuncommandEditor
 *
 * Get the UUID of the command for @editor
 *
 * Returns: (not nullable) (transfer none): the command UUID
 */
const char *
valent_runcommand_editor_get_uuid (ValentRuncommandEditor *editor)
{
  g_return_val_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor), NULL);

  return editor->uuid;
}

/**
 * valent_runcommand_editor_set_uuid:
 * @editor: a #ValentRuncommandEditor
 * @uuid: a command UUID
 *
 * Set the UUID of the command for @editor to @uuid.
 */
void
valent_runcommand_editor_set_uuid (ValentRuncommandEditor *editor,
                                   const char             *uuid)
{
  g_return_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor));

  if (uuid == NULL)
    uuid = "";

  valent_set_string (&editor->uuid, uuid);
}

/**
 * valent_runcommand_editor_clear:
 * @editor: a #ValentRuncommandEditor
 *
 * Clear the name, command and UUID of @editor.
 */
void
valent_runcommand_editor_clear (ValentRuncommandEditor *editor)
{
  g_return_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor));

  valent_runcommand_editor_set_uuid (editor, "");
  gtk_editable_set_text (GTK_EDITABLE (editor->name_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (editor->command_entry), "");
}

