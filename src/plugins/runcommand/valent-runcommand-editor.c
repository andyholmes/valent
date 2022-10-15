// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-editor"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

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
on_browse_response (GtkDialog              *dialog,
                    int                     response_id,
                    ValentRuncommandEditor *editor)
{
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
      g_autoptr (GFile) file = NULL;

      file = gtk_file_chooser_get_file (chooser);
      valent_runcommand_editor_set_command (editor, g_file_peek_path (file));
    }
}

static void
on_browse_command (GtkEntry               *entry,
                   GtkEntryIconPosition    icon_pos,
                   ValentRuncommandEditor *editor)
{
  GtkFileChooserNative *native;
  GtkFileFilter *filter;

  g_assert (VALENT_IS_RUNCOMMAND_EDITOR (editor));

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "application/*");

  native = g_object_new (GTK_TYPE_FILE_CHOOSER_NATIVE,
                         "title",         _("Select Command"),
                         "accept-label",  _("Select"),
                         "cancel-label",  _("Cancel"),
                         "filter",        filter,
                         "modal",         TRUE,
                         "transient-for", editor,
                         NULL);

  g_signal_connect (native,
                    "response",
                    G_CALLBACK (on_browse_response),
                    editor);

  gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
}

static void
on_entry_changed (GtkEntry               *entry,
                  ValentRuncommandEditor *editor)
{
  const char *command;
  const char *name;

  command = valent_runcommand_editor_get_command (editor);
  name = valent_runcommand_editor_get_name (editor);

  if (g_utf8_strlen (name, -1) > 0 && g_utf8_strlen (command, -1) > 0)
    gtk_widget_set_sensitive (GTK_WIDGET (editor->save_button), TRUE);
  else
    gtk_widget_set_sensitive (GTK_WIDGET (editor->save_button), FALSE);
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

  gtk_widget_class_bind_template_callback (widget_class, on_browse_command);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);
}

static void
valent_runcommand_editor_init (ValentRuncommandEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  if (!valent_in_flatpak ())
    {
      gtk_entry_set_icon_from_icon_name (self->command_entry,
                                         GTK_ENTRY_ICON_SECONDARY,
                                         "folder-symbolic");
    }

  self->uuid = g_strdup ("");
}

GtkDialog *
valent_runcommand_editor_new (void)
{
  return g_object_new (VALENT_TYPE_RUNCOMMAND_EDITOR,
                       "use-header-bar", TRUE,
                       NULL);
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

