// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-runcommand-editor"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-preferences-command-editor.h"

struct _ValentPreferencesCommandEditor
{
  AdwDialog    parent_instance;

  char        *uuid;
  GVariant    *command;

  /* template */
  AdwEntryRow *argv_entry;
  AdwEntryRow *name_entry;
};

G_DEFINE_TYPE (ValentPreferencesCommandEditor, valent_preferences_command_editor, ADW_TYPE_DIALOG)

typedef enum {
  PROP_COMMAND = 1,
  PROP_UUID,
} ValentPreferencesCommandEditorProperty;

static GParamSpec *properties[PROP_UUID + 1] = { NULL, };


static void
on_entry_activated (AdwEntryRow                    *entry,
                    ValentPreferencesCommandEditor *self)
{
  g_assert (VALENT_IS_PREFERENCES_COMMAND_EDITOR (self));

  gtk_widget_activate_action (GTK_WIDGET (self), "editor.save", NULL);
}

static void
on_entry_changed (AdwEntryRow                    *entry,
                  ValentPreferencesCommandEditor *self)
{
  const char *argv_text;
  const char *name_text;

  g_assert (entry == NULL || ADW_IS_ENTRY_ROW (entry));
  g_assert (VALENT_IS_PREFERENCES_COMMAND_EDITOR (self));

  argv_text = gtk_editable_get_text (GTK_EDITABLE (self->argv_entry));
  name_text = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));

  if (argv_text != NULL && *argv_text != '\0' &&
      name_text != NULL && *name_text != '\0')
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "editor.save", TRUE);
  else
    gtk_widget_action_set_enabled (GTK_WIDGET (self), "editor.save", FALSE);
}

/*
 * GAction
 */
static void
editor_remove_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
  ValentPreferencesCommandEditor *self = VALENT_PREFERENCES_COMMAND_EDITOR (widget);

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
  ValentPreferencesCommandEditor *self = VALENT_PREFERENCES_COMMAND_EDITOR (widget);
  GVariant *command = NULL;
  const char *name_text = NULL;
  const char *argv_text = NULL;

  g_assert (VALENT_IS_PREFERENCES_COMMAND_EDITOR (self));

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
valent_preferences_command_editor_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_PREFERENCES_COMMAND_EDITOR);

  G_OBJECT_CLASS (valent_preferences_command_editor_parent_class)->dispose (object);
}

static void
valent_preferences_command_editor_finalize (GObject *object)
{
  ValentPreferencesCommandEditor *self = VALENT_PREFERENCES_COMMAND_EDITOR (object);

  g_clear_pointer (&self->command, g_variant_unref);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (valent_preferences_command_editor_parent_class)->finalize (object);
}

static void
valent_preferences_command_editor_get_property (GObject    *object,
                                                guint       prop_id,
                                                GValue     *value,
                                                GParamSpec *pspec)
{
  ValentPreferencesCommandEditor *self = VALENT_PREFERENCES_COMMAND_EDITOR (object);

  switch ((ValentPreferencesCommandEditorProperty)prop_id)
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
valent_preferences_command_editor_set_property (GObject      *object,
                                                guint         prop_id,
                                                const GValue *value,
                                                GParamSpec   *pspec)
{
  ValentPreferencesCommandEditor *self = VALENT_PREFERENCES_COMMAND_EDITOR (object);

  switch ((ValentPreferencesCommandEditorProperty)prop_id)
    {
    case PROP_COMMAND:
      valent_preferences_command_editor_set_command (self, g_value_get_variant (value));
      break;

    case PROP_UUID:
      valent_preferences_command_editor_set_uuid (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_preferences_command_editor_class_init (ValentPreferencesCommandEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_preferences_command_editor_dispose;
  object_class->finalize = valent_preferences_command_editor_finalize;
  object_class->get_property = valent_preferences_command_editor_get_property;
  object_class->set_property = valent_preferences_command_editor_set_property;

  properties [PROP_COMMAND] =
    g_param_spec_variant ("command", NULL, NULL,
                          G_VARIANT_TYPE_VARDICT,
                          NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_UUID] =
    g_param_spec_string ("uuid", NULL, NULL,
                         "",
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-preferences-command-editor.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesCommandEditor, argv_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesCommandEditor, name_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);

  gtk_widget_class_install_action (widget_class, "editor.remove", NULL, editor_remove_action);
  gtk_widget_class_install_action (widget_class, "editor.save", NULL, editor_save_action);
}

static void
valent_preferences_command_editor_init (ValentPreferencesCommandEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GVariant *
valent_preferences_command_editor_get_command (ValentPreferencesCommandEditor *editor)
{
  g_return_val_if_fail (VALENT_IS_PREFERENCES_COMMAND_EDITOR (editor), NULL);

  return editor->command;
}

void
valent_preferences_command_editor_set_command (ValentPreferencesCommandEditor *editor,
                                               GVariant                       *command)
{
  const char *name_text = "";
  const char *argv_text = "";

  g_return_if_fail (VALENT_IS_PREFERENCES_COMMAND_EDITOR (editor));
  g_return_if_fail (command == NULL || g_variant_is_of_type (command, G_VARIANT_TYPE_VARDICT));

  g_clear_pointer (&editor->command, g_variant_unref);
  if (command != NULL)
    {
      editor->command = g_variant_ref_sink (command);
      g_variant_lookup (editor->command, "name", "&s", &name_text);
      g_variant_lookup (editor->command, "command", "&s", &argv_text);
    }

  gtk_editable_set_text (GTK_EDITABLE (editor->name_entry), name_text);
  gtk_editable_set_text (GTK_EDITABLE (editor->argv_entry), argv_text);
  gtk_widget_action_set_enabled (GTK_WIDGET (editor), "editor.remove",
                                 editor->command != NULL);

  g_object_notify_by_pspec (G_OBJECT (editor), properties [PROP_COMMAND]);
}

const char *
valent_preferences_command_editor_get_uuid (ValentPreferencesCommandEditor *editor)
{
  g_return_val_if_fail (VALENT_IS_PREFERENCES_COMMAND_EDITOR (editor), NULL);

  return editor->uuid;
}

void
valent_preferences_command_editor_set_uuid (ValentPreferencesCommandEditor *editor,
                                            const char                     *uuid)
{
  g_return_if_fail (VALENT_IS_PREFERENCES_COMMAND_EDITOR (editor));
  g_return_if_fail (uuid != NULL);

  if (g_set_str (&editor->uuid, uuid))
    g_object_notify_by_pspec (G_OBJECT (editor), properties [PROP_UUID]);
}

