// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-preferences-window"

#include "config.h"

#include <glib/gi18n.h>
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

#include "valent-application-plugin.h"
#include "valent-preferences-page.h"
#include "valent-preferences-window.h"


struct _ValentPreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

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

G_DEFINE_FINAL_TYPE (ValentPreferencesWindow, valent_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)


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
               ValentPreferencesWindow *self)
{
  const char *name = NULL;

  g_assert (GTK_IS_EDITABLE (editable));
  g_assert (VALENT_IS_PREFERENCES_WINDOW (self));

  name = gtk_editable_get_text (editable);

  if (name == NULL || *name == '\0')
    return;

  g_settings_set_string (self->settings, "name", name);
}

static void
on_settings_changed (GSettings               *settings,
                     const char              *key,
                     ValentPreferencesWindow *self)
{
  const char *text = NULL;
  g_autofree char *name = NULL;

  g_assert (G_IS_SETTINGS (settings));
  g_assert (key != NULL && *key != '\0');
  g_assert (VALENT_IS_PREFERENCES_WINDOW (self));

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
      GtkWidget *sw;

      if (!peas_engine_provides_extension (engine, info, extension.gtype))
        continue;

      row = g_object_new (ADW_TYPE_ACTION_ROW,
                          "title", extension.title,
                          NULL);
      adw_expander_row_add_row (ADW_EXPANDER_ROW (plugin_row), row);

      sw = g_object_new (GTK_TYPE_SWITCH,
                         "active",  TRUE,
                         "valign",  GTK_ALIGN_CENTER,
                         NULL);
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), sw);
      adw_action_row_set_activatable_widget (ADW_ACTION_ROW (row), sw);

      domain = valent_context_new (NULL, extension.domain, NULL);
      context = valent_context_get_plugin_context (domain, info);
      settings = valent_context_create_settings (context,
                                                 "ca.andyholmes.Valent.Plugin");
      g_settings_bind (settings, "enabled",
                       sw,       "active",
                       G_SETTINGS_BIND_DEFAULT);

      g_object_set_data_full (G_OBJECT (row),
                              "plugin-settings",
                              g_steal_pointer (&settings),
                              g_object_unref);
    }
}

static void
on_load_plugin (PeasEngine              *engine,
                PeasPluginInfo          *info,
                ValentPreferencesWindow *self)
{
  GtkWidget *row = NULL;
  const char *module;
  const char *title;
  const char *subtitle;
  const char *icon_name;

  g_assert (PEAS_IS_ENGINE (engine));
  g_assert (info != NULL);
  g_assert (VALENT_IS_PREFERENCES_WINDOW (self));

  engine = valent_get_plugin_engine ();
  module = peas_plugin_info_get_module_name (info);
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
      peas_engine_provides_extension (engine, info, VALENT_TYPE_SESSION_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_PREFERENCES_PAGE))
    {
      row = g_object_new (ADW_TYPE_EXPANDER_ROW,
                          "icon-name", icon_name,
                          "title",     title,
                          "subtitle",  subtitle,
                          NULL);

      plugin_row_add_extensions (ADW_EXPANDER_ROW (row), info);

      gtk_list_box_insert (self->plugin_list, row, -1);
      g_hash_table_insert (self->rows, info, g_object_ref (row));
    }

  /* Preferences Page */
  if (peas_engine_provides_extension (engine,
                                      info,
                                      VALENT_TYPE_PREFERENCES_PAGE))
    {
      PeasExtension *page;
      GtkWidget *button;

      button = g_object_new (GTK_TYPE_BUTTON,
                             "action-target", g_variant_new_string (module),
                             "action-name",   "win.page",
                             "icon-name",     "emblem-system-symbolic",
                             "valign",        GTK_ALIGN_CENTER,
                             NULL);
#if ADW_CHECK_VERSION (1, 4, 0)
      adw_expander_row_add_suffix (ADW_EXPANDER_ROW (row), button);
#else
      adw_expander_row_add_action (ADW_EXPANDER_ROW (row), button);
#endif

      page = peas_engine_create_extension (engine,
                                           info,
                                           VALENT_TYPE_PREFERENCES_PAGE,
                                           "name",      module,
                                           "icon-name", icon_name,
                                           "title",     title,
                                           NULL);
      adw_preferences_window_add (ADW_PREFERENCES_WINDOW (self),
                                  ADW_PREFERENCES_PAGE (page));
      g_hash_table_insert (self->pages, info, g_object_ref (page));
    }
}

static void
on_unload_plugin (PeasEngine              *engine,
                  PeasPluginInfo          *info,
                  ValentPreferencesWindow *self)
{
  g_autoptr (AdwPreferencesPage) page = NULL;
  g_autoptr (GtkWidget) row = NULL;

  if (g_hash_table_steal_extended (self->pages, info, NULL, (void **)&page))
    adw_preferences_window_remove (ADW_PREFERENCES_WINDOW (self), page);

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
  AdwPreferencesWindow *window = ADW_PREFERENCES_WINDOW (widget);
  const char *module;

  module = g_variant_get_string (parameter, NULL);
  adw_preferences_window_set_visible_page_name (window, module);
}

/*
 * GObject
 */
static void
valent_preferences_window_constructed (GObject *object)
{
  ValentPreferencesWindow *self = VALENT_PREFERENCES_WINDOW (object);
  g_autofree char *name = NULL;
  PeasEngine *engine = valent_get_plugin_engine ();
  const GList *plugins = NULL;

  /* Application Settings */
  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_signal_connect_object (self->settings,
                           "changed::name",
                           G_CALLBACK (on_settings_changed),
                           self, 0);
  name = g_settings_get_string (self->settings, "name");
  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), name);

  /* Application Plugins */
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    {
      if (peas_plugin_info_is_loaded (iter->data))
        on_load_plugin (engine, iter->data, self);
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

  G_OBJECT_CLASS (valent_preferences_window_parent_class)->constructed (object);
}

static void
valent_preferences_window_dispose (GObject *object)
{
  ValentPreferencesWindow *self = VALENT_PREFERENCES_WINDOW (object);

  g_signal_handlers_disconnect_by_data (valent_get_plugin_engine (), self);
  g_clear_object (&self->settings);

  gtk_widget_dispose_template (GTK_WIDGET (object),
                               VALENT_TYPE_PREFERENCES_WINDOW);

  G_OBJECT_CLASS (valent_preferences_window_parent_class)->dispose (object);
}

static void
valent_preferences_window_finalize (GObject *object)
{
  ValentPreferencesWindow *self = VALENT_PREFERENCES_WINDOW (object);

  g_clear_pointer (&self->pages, g_hash_table_unref);
  g_clear_pointer (&self->rows, g_hash_table_unref);

  G_OBJECT_CLASS (valent_preferences_window_parent_class)->finalize (object);
}

static void
valent_preferences_window_class_init (ValentPreferencesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_preferences_window_constructed;
  object_class->dispose = valent_preferences_window_dispose;
  object_class->finalize = valent_preferences_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-preferences-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, general_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, main_page);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, plugin_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, plugin_list);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, name_entry);

  gtk_widget_class_bind_template_callback (widget_class, on_name_apply);

  gtk_widget_class_install_action (widget_class, "win.page", "s", page_action);

  /* ... */
  extensions[EXTEN_APPLICATION_PLUGIN] =
    (ExtensionDescription){
      VALENT_TYPE_APPLICATION_PLUGIN,
      N_("Global"),
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
valent_preferences_window_init (ValentPreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_sort_func (self->plugin_list, plugin_list_sort, NULL, NULL);

  self->pages = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
  self->rows = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);
}

