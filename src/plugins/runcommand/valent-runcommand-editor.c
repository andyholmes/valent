// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-editor"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-runcommand-editor.h"


struct _ValentRuncommandEditor
{
  GtkWindow    parent_instance;

  char        *uuid;
  GVariant    *command;

  /* template */
  GtkWidget   *save_button;
  AdwEntryRow *argv_entry;
  AdwEntryRow *name_entry;
  GtkWidget   *remove_group;
};

G_DEFINE_TYPE (ValentRuncommandEditor, valent_runcommand_editor, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_COMMAND,
  PROP_UUID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
on_entry_changed (AdwEntryRow            *entry,
                  ValentRuncommandEditor *self)
{
  const char *argv_text;
  const char *name_text;

  g_assert (entry == NULL || ADW_IS_ENTRY_ROW (entry));
  g_assert (VALENT_IS_RUNCOMMAND_EDITOR (self));

  argv_text = gtk_editable_get_text (GTK_EDITABLE (self->argv_entry));
  name_text = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));

  if (argv_text != NULL && *argv_text != '\0' &&
      name_text != NULL && *name_text != '\0')
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "editor.save", TRUE);
  else
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "editor.save", FALSE);
}

static void
valent_runcommand_editor_sync (ValentRuncommandEditor *self)
{
  const char *name_text = "";
  const char *argv_text = "";

  g_assert (VALENT_IS_RUNCOMMAND_EDITOR (self));

  /* Parse command */
  if (self->command != NULL)
    {
      g_variant_lookup (self->command, "name", "&s", &name_text);
      g_variant_lookup (self->command, "command", "&s", &argv_text);
    }

  /* Update editor content */
  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), name_text);
  gtk_editable_set_text (GTK_EDITABLE (self->argv_entry), argv_text);
  gtk_widget_set_visible (self->remove_group, self->command != NULL);
  on_entry_changed (NULL, self);
}

/*
 * GAction
 */
static void
editor_cancel_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
  ValentRuncommandEditor *self = VALENT_RUNCOMMAND_EDITOR (widget);

  valent_runcommand_editor_sync (self);
  g_object_notify_by_pspec (G_OBJECT (widget), properties [PROP_COMMAND]);
}

static void
editor_remove_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
  ValentRuncommandEditor *self = VALENT_RUNCOMMAND_EDITOR (widget);

  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->argv_entry), "");

  g_clear_pointer (&self->command, g_variant_unref);
  g_object_notify_by_pspec (G_OBJECT (widget), properties [PROP_COMMAND]);
}

static void
editor_save_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  ValentRuncommandEditor *self = VALENT_RUNCOMMAND_EDITOR (widget);
  GVariant *command = NULL;
  const char *name_text = NULL;
  const char *argv_text = NULL;

  g_assert (VALENT_IS_RUNCOMMAND_EDITOR (self));

  name_text = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
  argv_text = gtk_editable_get_text (GTK_EDITABLE (self->argv_entry));

  g_return_if_fail (argv_text != NULL && *argv_text != '\0' &&
                    name_text != NULL && *name_text != '\0');

  command = g_variant_new_parsed ("{"
                                    "'name': <%s>, "
                                    "'command': <%s>"
                                  "}",
                                  name_text,
                                  argv_text);

  g_clear_pointer (&self->command, g_variant_unref);
  self->command = g_variant_ref_sink (command);
  g_object_notify_by_pspec (G_OBJECT (widget), properties [PROP_COMMAND]);
}

/*
 * GObject
 */
static void
valent_runcommand_editor_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_RUNCOMMAND_EDITOR);

  G_OBJECT_CLASS (valent_runcommand_editor_parent_class)->dispose (object);
}

static void
valent_runcommand_editor_finalize (GObject *object)
{
  ValentRuncommandEditor *self = VALENT_RUNCOMMAND_EDITOR (object);

  g_clear_pointer (&self->command, g_variant_unref);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (valent_runcommand_editor_parent_class)->finalize (object);
}

static void
valent_runcommand_editor_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ValentRuncommandEditor *self = VALENT_RUNCOMMAND_EDITOR (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_variant (value, self->command);
      break;

    case PROP_UUID:
      g_value_set_string (value, self->uuid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_runcommand_editor_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ValentRuncommandEditor *self = VALENT_RUNCOMMAND_EDITOR (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      valent_runcommand_editor_set_command (self, g_value_get_variant (value));
      break;

    case PROP_UUID:
      valent_runcommand_editor_set_uuid (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_runcommand_editor_class_init (ValentRuncommandEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_runcommand_editor_dispose;
  object_class->finalize = valent_runcommand_editor_finalize;
  object_class->get_property = valent_runcommand_editor_get_property;
  object_class->set_property = valent_runcommand_editor_set_property;

  /**
   * ValentRuncommandEditor:command: (getter get_command) (setter set_command)
   *
   * The command the editor is operating on.
   *
   * Since: 1.0
   */
  properties [PROP_COMMAND] =
    g_param_spec_variant ("command", NULL, NULL,
                          G_VARIANT_TYPE_VARDICT,
                          NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentRuncommandEditor:uuid: (getter get_uuid) (setter set_uuid)
   *
   * The uuid the editor is operating on.
   *
   * Since: 1.0
   */
  properties [PROP_UUID] =
    g_param_spec_string ("uuid", NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/runcommand/valent-runcommand-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, save_button);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, argv_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, name_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentRuncommandEditor, remove_group);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);

  gtk_widget_class_install_action (widget_class, "editor.cancel", NULL, editor_cancel_action);
  gtk_widget_class_install_action (widget_class, "editor.remove", NULL, editor_remove_action);
  gtk_widget_class_install_action (widget_class, "editor.save", NULL, editor_save_action);
}

static void
valent_runcommand_editor_init (ValentRuncommandEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * valent_runcommand_editor_get_command:
 * @editor: a `ValentRuncommandEditor`
 *
 * Get the command the editor is operating on.
 *
 * Returns: (transfer none) (nullable): the command
 */
GVariant *
valent_runcommand_editor_get_command (ValentRuncommandEditor *editor)
{
  g_return_val_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor), NULL);

  return editor->command;
}

/**
 * valent_runcommand_editor_set_command:
 * @editor: a `ValentRuncommandEditor`
 * @command: a command entry
 *
 * Set the command for the editor to operate on.
 */
void
valent_runcommand_editor_set_command (ValentRuncommandEditor *editor,
                                      GVariant               *command)
{
  g_return_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor));
  g_return_if_fail (command == NULL || g_variant_is_of_type (command, G_VARIANT_TYPE_VARDICT));

  /* Update the property */
  g_clear_pointer (&editor->command, g_variant_unref);
  editor->command = command ? g_variant_ref_sink (command) : NULL;
  g_object_notify_by_pspec (G_OBJECT (editor), properties [PROP_COMMAND]);

  valent_runcommand_editor_sync (editor);
}

/**
 * valent_runcommand_editor_get_uuid:
 * @editor: a `ValentRuncommandEditor`
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
 * @editor: a `ValentRuncommandEditor`
 * @uuid: a command UUID
 *
 * Set the UUID of the command for @editor to @uuid.
 */
void
valent_runcommand_editor_set_uuid (ValentRuncommandEditor *editor,
                                   const char             *uuid)
{
  g_return_if_fail (VALENT_IS_RUNCOMMAND_EDITOR (editor));
  g_return_if_fail (uuid != NULL);

  if (g_set_str (&editor->uuid, uuid))
    g_object_notify_by_pspec (G_OBJECT (editor), properties [PROP_UUID]);
}

