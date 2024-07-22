// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-preferences-dialog"

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>
#include <libvalent-contacts.h>
#include <libvalent-device.h>
#include <libvalent-input.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>
#include <libvalent-notifications.h>
#include <libvalent-session.h>

#include "valent-preferences-dialog.h"


struct _ValentPreferencesDialog
{
  AdwPreferencesDialog  parent_instance;

  GSettings            *settings;
  GHashTable           *pages;
  GHashTable           *rows;

  /* template */
  AdwPreferencesPage   *main_page;

  AdwPreferencesGroup  *general_group;
  AdwEntryRow          *name_entry;

  AdwPreferencesGroup  *plugin_group;
  GtkListBox           *plugin_list;
};

G_DEFINE_FINAL_TYPE (ValentPreferencesDialog, valent_preferences_dialog, ADW_TYPE_PREFERENCES_DIALOG)


typedef struct
{
  GType  gtype;
  char  *title;
  char  *domain;
} ExtensionDescription;

enum {
  EXTEN_APPLICATION_PLUGIN,
  EXTEN_CHANNEL_SERVICE,
  EXTEN_CLIPBOARD_ADAPTER,
  EXTEN_CONTACTS_ADAPTER,
  EXTEN_INPUT_ADAPTER,
  EXTEN_MEDIA_ADAPTER,
  EXTEN_MIXER_ADAPTER,
  EXTEN_NOTIFICATION_ADAPTER,
  EXTEN_SESSION_ADAPTER,
  N_EXTENSIONS,
};

static ExtensionDescription extensions[N_EXTENSIONS] = { 0, };


static int
plugin_list_sort (GtkListBoxRow *row1,
                  GtkListBoxRow *row2,
                  gpointer       user_data)
{
  if G_UNLIKELY (!ADW_IS_PREFERENCES_ROW (row1) ||
                 !ADW_IS_PREFERENCES_ROW (row2))
    return 0;

  return g_utf8_collate (adw_preferences_row_get_title ((AdwPreferencesRow *)row1),
                         adw_preferences_row_get_title ((AdwPreferencesRow *)row2));
}

/*
 * Device Name Callbacks
 */
static void
on_name_apply (GtkEditable             *editable,
               ValentPreferencesDialog *self)
{
  const char *name = NULL;

  g_assert (GTK_IS_EDITABLE (editable));
  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  name = gtk_editable_get_text (editable);

  if (name == NULL || *name == '\0')
    return;

  g_settings_set_string (self->settings, "name", name);
}

static void
on_settings_changed (GSettings               *settings,
                     const char              *key,
                     ValentPreferencesDialog *self)
{
  const char *text = NULL;
  g_autofree char *name = NULL;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL && *key != '\0');
  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  name = g_settings_get_string (self->settings, "name");
  text = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));

  if (g_strcmp0 (text, name) != 0)
    gtk_editable_set_text (GTK_EDITABLE (self->name_entry), name);
}

/*
 * PeasEngine Callbacks
 */
static void
plugin_row_add_extensions (AdwExpanderRow *plugin_row,
                           PeasPluginInfo *info)
{
  PeasEngine *engine = valent_get_plugin_engine ();
  GtkWidget *row;

  for (unsigned int i = 0; i < N_EXTENSIONS; i++)
    {
      ExtensionDescription extension = extensions[i];
      g_autoptr (ValentContext) domain = NULL;
      g_autoptr (ValentContext) context = NULL;
      g_autoptr (GSettings) settings = NULL;

      if (!peas_engine_provides_extension (engine, info, extension.gtype))
        continue;

      row = g_object_new (ADW_TYPE_SWITCH_ROW,
                          "title",      _(extension.title),
                          "selectable", FALSE,
                          NULL);
      adw_expander_row_add_row (ADW_EXPANDER_ROW (plugin_row), row);

      domain = valent_context_new (NULL, extension.domain, NULL);
      context = valent_context_get_plugin_context (domain, info);
      settings = valent_context_create_settings (context,
                                                 "ca.andyholmes.Valent.Plugin");
      g_settings_bind (settings, "enabled",
                       row,      "active",
                       G_SETTINGS_BIND_DEFAULT);
      adw_switch_row_set_active (ADW_SWITCH_ROW (row),
                                 g_settings_get_boolean (settings, "enabled"));
      g_object_set_data_full (G_OBJECT (row),
                              "plugin-settings",
                              g_steal_pointer (&settings),
                              g_object_unref);
    }
}

static void
on_load_plugin (PeasEngine              *engine,
                PeasPluginInfo          *info,
                ValentPreferencesDialog *self)
{
  GtkWidget *row = NULL;
  const char *title;
  const char *subtitle;
  const char *icon_name;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  if (peas_plugin_info_is_hidden (info))
    return;

  engine = valent_get_plugin_engine ();
  title = peas_plugin_info_get_name (info);
  subtitle = peas_plugin_info_get_description (info);
  icon_name = peas_plugin_info_get_icon_name (info);

  if (peas_engine_provides_extension (engine, info, VALENT_TYPE_APPLICATION_PLUGIN) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_CLIPBOARD_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_CONTACTS_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_INPUT_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_MEDIA_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_MIXER_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_NOTIFICATIONS_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_SESSION_ADAPTER))
    {
      GtkWidget *icon;

      row = g_object_new (ADW_TYPE_EXPANDER_ROW,
                          "title",      title,
                          "subtitle",   subtitle,
                          "selectable", FALSE,
                          NULL);
      icon = g_object_new (GTK_TYPE_IMAGE,
                           "accessible-role", GTK_ACCESSIBLE_ROLE_PRESENTATION,
                           "icon-name",       icon_name,
                           NULL);
      adw_expander_row_add_prefix (ADW_EXPANDER_ROW (row), icon);

      plugin_row_add_extensions (ADW_EXPANDER_ROW (row), info);

      gtk_list_box_insert (self->plugin_list, row, -1);
      g_hash_table_insert (self->rows, info, g_object_ref (row));
    }
}

static void
on_unload_plugin (PeasEngine              *engine,
                  PeasPluginInfo          *info,
                  ValentPreferencesDialog *self)
{
  g_autoptr (AdwPreferencesPage) page = NULL;
  g_autoptr (GtkWidget) row = NULL;

  if (g_hash_table_steal_extended (self->pages, info, NULL, (void **)&page))
    adw_preferences_dialog_remove (ADW_PREFERENCES_DIALOG (self), page);

  if (g_hash_table_steal_extended (self->rows, info, NULL, (void **)&row))
    gtk_list_box_remove (self->plugin_list, row);
}

/*
 * GActions
 */
static void
page_action (GtkWidget  *widget,
             const char *action_name,
             GVariant   *parameter)
{
  AdwPreferencesDialog *window = ADW_PREFERENCES_DIALOG (widget);
  const char *module;

  module = g_variant_get_string (parameter, NULL);
  adw_preferences_dialog_set_visible_page_name (window, module);
}

/*
 * GObject
 */
static void
valent_preferences_dialog_constructed (GObject *object)
{
  ValentPreferencesDialog *self = VALENT_PREFERENCES_DIALOG (object);
  g_autofree char *name = NULL;
  PeasEngine *engine = valent_get_plugin_engine ();
  unsigned int n_plugins = 0;

  /* Application Settings */
  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_signal_connect_object (self->settings,
                           "changed::name",
                           G_CALLBACK (on_settings_changed),
                           self, 0);
  name = g_settings_get_string (self->settings, "name");
  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), name);

  /* Application Plugins */
  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (engine));

  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) info = NULL;

      info = g_list_model_get_item (G_LIST_MODEL (engine), i);

      if (peas_plugin_info_is_loaded (info))
        on_load_plugin (engine, info, self);
    }

  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (on_load_plugin),
                           self,
                           G_CONNECT_AFTER);

  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_plugin),
                           self,
                           0);

  G_OBJECT_CLASS (valent_preferences_dialog_parent_class)->constructed (object);
}

static void
valent_preferences_dialog_dispose (GObject *object)
{
  ValentPreferencesDialog *self = VALENT_PREFERENCES_DIALOG (object);

  g_signal_handlers_disconnect_by_data (valent_get_plugin_engine (), self);
  g_clear_object (&self->settings);

  gtk_widget_dispose_template (GTK_WIDGET (object),
                               VALENT_TYPE_PREFERENCES_DIALOG);

  G_OBJECT_CLASS (valent_preferences_dialog_parent_class)->dispose (object);
}

static void
valent_preferences_dialog_finalize (GObject *object)
{
  ValentPreferencesDialog *self = VALENT_PREFERENCES_DIALOG (object);

  g_clear_pointer (&self->pages, g_hash_table_unref);
  g_clear_pointer (&self->rows, g_hash_table_unref);

  G_OBJECT_CLASS (valent_preferences_dialog_parent_class)->finalize (object);
}

static void
valent_preferences_dialog_class_init (ValentPreferencesDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_preferences_dialog_constructed;
  object_class->dispose = valent_preferences_dialog_dispose;
  object_class->finalize = valent_preferences_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-preferences-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, general_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, main_page);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, plugin_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, plugin_list);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, name_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_name_apply);

  gtk_widget_class_install_action (widget_class, "win.page", "s", page_action);

  /* ... */
  extensions[EXTEN_APPLICATION_PLUGIN] =
    (ExtensionDescription){
      VALENT_TYPE_APPLICATION_PLUGIN,
      N_("Application"),
      "application",
    };

  extensions[EXTEN_CHANNEL_SERVICE] =
    (ExtensionDescription){
      VALENT_TYPE_CHANNEL_SERVICE,
      N_("Device Connections"),
      "network",
    };

  extensions[EXTEN_CLIPBOARD_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_CLIPBOARD_ADAPTER,
      N_("Clipboard"),
      "clipboard",
    };

  extensions[EXTEN_CONTACTS_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_CONTACTS_ADAPTER,
      N_("Contacts"),
      "contacts",
    };

  extensions[EXTEN_INPUT_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_INPUT_ADAPTER,
      N_("Mouse and Keyboard"),
      "input",
    };

  extensions[EXTEN_MEDIA_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_MEDIA_ADAPTER,
      N_("Media Players"),
      "media",
    };

  extensions[EXTEN_MIXER_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_MIXER_ADAPTER,
      N_("Volume Control"),
      "mixer",
    };

  extensions[EXTEN_NOTIFICATION_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_NOTIFICATIONS_ADAPTER,
      N_("Notifications"),
      "notifications",
    };

  extensions[EXTEN_SESSION_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_SESSION_ADAPTER,
      N_("Session Manager"),
      "session",
    };
}

static void
valent_preferences_dialog_init (ValentPreferencesDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->plugin_list, plugin_list_sort, NULL, NULL);

  self->pages = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  self->rows = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}

