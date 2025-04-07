// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-preferences-dialog"

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

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
  GtkRevealer          *name_error;
  GtkLabel             *name_error_label;
  AdwPreferencesGroup  *network_address_group;
  GtkListBox           *network_address_list;
  AdwDialog            *network_address_dialog;
  GtkEditable          *network_address_entry;
  GtkButton            *network_address_add;

  AdwPreferencesPage   *plugin_page;
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
  if (valent_device_validate_name (name))
    {
      adw_entry_row_set_show_apply_button (self->name_entry, TRUE);
      gtk_widget_remove_css_class (GTK_WIDGET (self->name_entry), "warning");
      gtk_revealer_set_reveal_child (self->name_error, FALSE);
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->name_entry),
                                  GTK_ACCESSIBLE_STATE_INVALID);
      g_settings_set_string (self->settings, "name", name);
    }
  else
    {
      adw_entry_row_set_show_apply_button (self->name_entry, FALSE);
      gtk_widget_add_css_class (GTK_WIDGET (self->name_entry), "warning");
      gtk_revealer_set_reveal_child (self->name_error, TRUE);
      gtk_accessible_update_state (GTK_ACCESSIBLE (self->name_entry),
                                   GTK_ACCESSIBLE_STATE_INVALID,
                                   GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
      // FIXME: compel AT-devices to speak the forbidden punctuation
      gtk_accessible_announce (GTK_ACCESSIBLE (self->name_entry),
                               gtk_label_get_text (self->name_error_label),
                               GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
    }
}

static void
on_name_changed (GtkEditable             *editable,
                 ValentPreferencesDialog *self)
{
  const char *name = NULL;

  g_assert (GTK_IS_EDITABLE (editable));
  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  name = gtk_editable_get_text (editable);
  if (valent_device_validate_name (name))
    {
      adw_entry_row_set_show_apply_button (self->name_entry, TRUE);
      gtk_widget_remove_css_class (GTK_WIDGET (self->name_entry), "warning");
      gtk_revealer_set_reveal_child (self->name_error, FALSE);
      gtk_accessible_reset_state (GTK_ACCESSIBLE (self->name_entry),
                                  GTK_ACCESSIBLE_STATE_INVALID);
    }
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
  const char *title;
  const char *subtitle;
  const char *icon_name;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  if (peas_plugin_info_is_hidden (info))
    return;

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
      GtkWidget *row;
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
 * Manual Connections
 */
static gboolean
validate_host_entry (const char *input)
{
  g_autoptr (GSocketConnectable) net = NULL;

  if (input == NULL || *input == '\0')
    return FALSE;

  net = g_network_address_parse (input, 1716, NULL);
  if (net == NULL)
    return FALSE;

  return TRUE;
}

static void
on_network_address_activated (GtkWidget               *widget,
                              ValentPreferencesDialog *self)
{
  const char *hostname = NULL;

  g_assert (GTK_IS_EDITABLE (widget) || GTK_IS_BUTTON (widget));
  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  hostname = gtk_editable_get_text (GTK_EDITABLE (self->network_address_entry));
  if (validate_host_entry (hostname))
    {
      gtk_widget_activate_action (GTK_WIDGET (self),
                                  "network.add-address",
                                  "s", hostname);
      gtk_editable_set_text (GTK_EDITABLE (self->network_address_entry), "");
      gtk_widget_grab_focus (GTK_WIDGET (self->network_address_entry));
      adw_dialog_close (self->network_address_dialog);
    }
}

static void
on_network_address_changed (GtkEditable *editable,
                            GtkButton   *button)
{
  const char *hostname = NULL;

  g_assert (GTK_IS_EDITABLE (editable));
  g_assert (GTK_IS_BUTTON (button));

  hostname = gtk_editable_get_text (editable);
  if (validate_host_entry (hostname))
    {
      gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
      gtk_widget_remove_css_class (GTK_WIDGET (editable), "warning");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (editable),
                                  GTK_ACCESSIBLE_STATE_INVALID);
    }
  else if (hostname == NULL || *hostname == '\0')
    {
      gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
      gtk_widget_remove_css_class (GTK_WIDGET (editable), "warning");
      gtk_accessible_reset_state (GTK_ACCESSIBLE (editable),
                                  GTK_ACCESSIBLE_STATE_INVALID);
    }
  else
    {
      gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
      gtk_widget_add_css_class (GTK_WIDGET (editable), "warning");
      gtk_accessible_update_state (GTK_ACCESSIBLE (editable),
                                   GTK_ACCESSIBLE_STATE_INVALID,
                                   GTK_ACCESSIBLE_INVALID_TRUE,
                                   -1);
    }
}

static GtkWidget *
network_address_create_row (gpointer item,
                            gpointer user_data)
{
  GtkStringObject *string = GTK_STRING_OBJECT (item);
  GtkWidget *row, *button;
  const char *address;

  address = gtk_string_object_get_string (string);
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title",      gtk_string_object_get_string (string),
                      "use-markup", FALSE,
                      NULL);
  button = g_object_new (GTK_TYPE_BUTTON,
                         "icon-name",     "edit-delete-symbolic",
                         "action-target", g_variant_new_string (address),
                         "action-name",   "network.remove-address",
                         "tooltip-text",  _("Remove"),
                         "valign",        GTK_ALIGN_CENTER,
                         NULL);
  gtk_widget_add_css_class (button, "flat");
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), button);

  return row;
}

static void
on_device_addresses_changed (GSettings               *settings,
                             const char              *key,
                             ValentPreferencesDialog *self)
{
  g_auto (GStrv) addresses = NULL;
  g_autoptr (GtkStringList) model = NULL;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL && *key != '\0');
  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  addresses = g_settings_get_strv (self->settings, "device-addresses");
  model = gtk_string_list_new ((const char * const *)addresses);
  gtk_list_box_bind_model (self->network_address_list,
                           G_LIST_MODEL (model),
                           network_address_create_row,
                           NULL, NULL);
}

/*
 * GActions
 */
static void
add_address_action (GtkWidget  *widget,
                    const char *action_name,
                    GVariant   *parameter)
{
  ValentPreferencesDialog *self = VALENT_PREFERENCES_DIALOG (widget);
  g_auto (GStrv) addresses = NULL;
  const char *address = NULL;

  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  address = g_variant_get_string (parameter, NULL);
  addresses = g_settings_get_strv (self->settings, "device-addresses");
  if (!g_strv_contains ((const char * const *)addresses, address))
    {
      g_autoptr (GStrvBuilder) builder = NULL;

      builder = g_strv_builder_new ();
      g_strv_builder_add (builder, address);
      g_strv_builder_addv (builder, (const char **)addresses);

      g_clear_pointer (&addresses, g_strfreev);
      addresses = g_strv_builder_end (builder);
      g_settings_set_strv (self->settings, "device-addresses",
                           (const char * const *)addresses);
    }
}

static void
remove_address_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *parameter)
{
  ValentPreferencesDialog *self = VALENT_PREFERENCES_DIALOG (widget);
  g_auto (GStrv) addresses = NULL;
  const char *address = NULL;

  g_assert (VALENT_IS_PREFERENCES_DIALOG (self));

  address = g_variant_get_string (parameter, NULL);
  addresses = g_settings_get_strv (self->settings, "device-addresses");
  if (g_strv_contains ((const char * const *)addresses, address))
    {
      g_autoptr (GStrvBuilder) builder = NULL;

      builder = g_strv_builder_new ();
      for (size_t i = 0; addresses[i] != NULL; i++)
        {
          if (!g_str_equal (addresses[i], address))
            g_strv_builder_add (builder, addresses[i]);
        }

      g_clear_pointer (&addresses, g_strfreev);
      addresses = g_strv_builder_end (builder);
      g_settings_set_strv (self->settings, "device-addresses",
                           (const char * const *)addresses);
    }
}

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
  g_autofree char *error_label = NULL;
  g_autofree char *name = NULL;
  PeasEngine *engine = NULL;
  unsigned int n_plugins = 0;

  G_OBJECT_CLASS (valent_preferences_dialog_parent_class)->constructed (object);

  /* Device Name
   */
  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_signal_connect_object (self->settings,
                           "changed::name",
                           G_CALLBACK (on_settings_changed),
                           self, 0);
  name = g_settings_get_string (self->settings, "name");
  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), name);

  // TRANSLATORS: %s is a list of forbidden characters
  error_label = g_strdup_printf (_("The device name must not contain "
                                   "punctuation or brackets, including %s"),
                                 "<b><tt>\"',;:.!?()[]&lt;&gt;</tt></b>");
  gtk_label_set_markup (self->name_error_label, error_label);

  /* Manual Device Setup
   */
  g_signal_connect_object (self->settings,
                           "changed::device-addresses",
                           G_CALLBACK (on_device_addresses_changed),
                           self,
                           G_CONNECT_DEFAULT);
  on_device_addresses_changed (self->settings, "device-addresses", self);

  /* Plugins
   */
  engine = valent_get_plugin_engine ();
  g_signal_connect_object (engine,
                           "load-plugin",
                           G_CALLBACK (on_load_plugin),
                           self,
                           G_CONNECT_AFTER);
  g_signal_connect_object (engine,
                           "unload-plugin",
                           G_CALLBACK (on_unload_plugin),
                           self,
                           G_CONNECT_DEFAULT);

  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (engine));
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) info = NULL;

      info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      if (peas_plugin_info_is_loaded (info))
        on_load_plugin (engine, info, self);
    }
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

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-preferences-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, general_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, main_page);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, plugin_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, plugin_list);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, name_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, name_error);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, name_error_label);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, network_address_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, network_address_list);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, network_address_dialog);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, network_address_add);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesDialog, network_address_entry);
  gtk_widget_class_bind_template_callback (widget_class, on_name_apply);
  gtk_widget_class_bind_template_callback (widget_class, on_name_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_network_address_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_network_address_changed);

  gtk_widget_class_install_action (widget_class, "network.add-address", "s", add_address_action);
  gtk_widget_class_install_action (widget_class, "network.remove-address", "s", remove_address_action);
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

