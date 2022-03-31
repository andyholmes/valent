// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-preferences-window"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>
#include <libvalent-contacts.h>
#include <libvalent-input.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>
#include <libvalent-notifications.h>
#include <libvalent-session.h>

#include "valent-preferences-page.h"
#include "valent-preferences-window.h"
#include "valent-preferences-window-private.h"


struct _ValentPreferencesWindow
{
  AdwPreferencesWindow  parent_instance;

  GSettings            *settings;

  /* Template widgets */
  AdwPreferencesPage   *main_page;

  AdwPreferencesGroup  *general_group;

  GtkDialog            *rename_dialog;
  GtkEntry             *rename_entry;
  GtkLabel             *rename_label;
  GtkButton            *rename_button;

  AdwPreferencesGroup  *plugin_group;
  GtkListBox           *plugin_list;

  GHashTable           *pages;
  GHashTable           *rows;
};

G_DEFINE_TYPE (ValentPreferencesWindow, valent_preferences_window, ADW_TYPE_PREFERENCES_WINDOW)


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
  EXTEN_CONTACT_STORE_PROVIDER,
  EXTEN_INPUT_ADAPTER,
  EXTEN_MEDIA_PLAYER_PROVIDER,
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
 * Device Name Dialog
 */
static void
on_rename_entry_changed (GtkEntry                *entry,
                         ValentPreferencesWindow *self)
{
  const char *name = NULL;
  const char *new_name = NULL;

  name = gtk_label_get_text (self->rename_label);
  new_name = gtk_editable_get_text (GTK_EDITABLE (entry));

  gtk_widget_set_sensitive (GTK_WIDGET (self->rename_button),
                            (g_strcmp0 (name, new_name) != 0));
}

static void
on_rename_dialog_open (AdwActionRow            *row,
                       ValentPreferencesWindow *self)
{
  g_autofree char *name = NULL;

  name = g_settings_get_string (self->settings, "name");
  gtk_editable_set_text (GTK_EDITABLE (self->rename_entry), name);

  gtk_window_present_with_time (GTK_WINDOW (self->rename_dialog),
                                GDK_CURRENT_TIME);
}

static void
on_rename_dialog_response (GtkDialog               *dialog,
                           GtkResponseType          response_id,
                           ValentPreferencesWindow *self)
{
  if (response_id == GTK_RESPONSE_OK)
    {
      const char *name;

      name = gtk_editable_get_text (GTK_EDITABLE (self->rename_entry));
      g_settings_set_string (self->settings, "name", name);
    }

  gtk_widget_hide (GTK_WIDGET (dialog));
}

/*
 * PeasEngine Callbacks
 */
static void
plugin_row_add_extensions (AdwExpanderRow *plugin_row,
                           PeasPluginInfo *info)
{
  PeasEngine *engine = valent_get_engine ();
  const char *module_name = peas_plugin_info_get_module_name (info);
  GtkWidget *row;

  for (unsigned int i = 0; i < N_EXTENSIONS; i++)
    {
      ExtensionDescription extension = extensions[i];
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

      settings = valent_component_new_settings (extension.domain, module_name);
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

  engine = valent_get_engine ();
  module = peas_plugin_info_get_module_name (info);
  title = peas_plugin_info_get_name (info);
  subtitle = peas_plugin_info_get_description (info);
  icon_name = peas_plugin_info_get_icon_name (info);

  if (peas_engine_provides_extension (engine, info, VALENT_TYPE_APPLICATION_PLUGIN) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_CLIPBOARD_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_CHANNEL_SERVICE) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_CONTACT_STORE_PROVIDER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_INPUT_ADAPTER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_MEDIA_PLAYER_PROVIDER) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_MIXER_CONTROL) ||
      peas_engine_provides_extension (engine, info, VALENT_TYPE_NOTIFICATION_SOURCE) ||
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
      adw_expander_row_add_action (ADW_EXPANDER_ROW (row), button);

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
 * HACK: The view switcher controls don't scale well with arbitrary numbers of
 *       plugins, so attempt to hide it and replace the functionality with a
 *       "previous" button.
 */
static AdwHeaderBar *
find_header_bar (GtkWidget *widget)
{
  GtkWidget *child;

  if (ADW_IS_HEADER_BAR (widget))
    return ADW_HEADER_BAR (widget);

  child = gtk_widget_get_first_child (widget);

  while (child && !ADW_IS_HEADER_BAR (child))
    {
      AdwHeaderBar *headerbar;

      if ((headerbar = find_header_bar (child)))
        return headerbar;

      child = gtk_widget_get_next_sibling (child);
    }

  return ADW_HEADER_BAR (child);
}

static gboolean
hide_view_switcher_bar (GtkWidget *widget)
{
  GtkWidget *child = NULL;

  if (ADW_IS_VIEW_SWITCHER_BAR (widget))
    {
      gtk_widget_set_visible (widget, FALSE);
      return TRUE;
    }

  child = gtk_widget_get_first_child (widget);

  while (child)
    {
      if (hide_view_switcher_bar (child))
        return TRUE;

      child = gtk_widget_get_next_sibling (child);
    }

  return FALSE;
}

/**
 * valent_preferences_window_modify:
 * @window: a #AdwPreferencesWindow
 *
 * Try to modify a [class@Adw.PreferencesWindow] to hide the bottom view swither
 * for cases where the number of pages exceeds the number that can be reasonably
 * handled.
 *
 * Returns: %TRUE if successful, or %FALSE if not
 */
gboolean
valent_preferences_window_modify (AdwPreferencesWindow *window)
{
  AdwHeaderBar *headerbar = NULL;
  GtkWidget *button;

  g_assert (ADW_IS_PREFERENCES_WINDOW (window));

  /* Add a "previous" button to the headerbar */
  if ((headerbar = find_header_bar (GTK_WIDGET (window))) == NULL)
    return FALSE;

  button = g_object_new (GTK_TYPE_BUTTON,
                         "action-name",  "win.previous",
                         "icon-name",    "go-previous-symbolic",
                         "tooltip-text", _("Previous"),
                         NULL);
  adw_header_bar_pack_start (headerbar, button);

  /* Attempt to find and hide the AdwViewSwitcherBar */
  if (!hide_view_switcher_bar (GTK_WIDGET (window)))
    return FALSE;

  return TRUE;
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

static void
previous_action (GtkWidget  *widget,
                 const char *action_name,
                 GVariant   *parameter)
{
  AdwPreferencesWindow *window = ADW_PREFERENCES_WINDOW (widget);
  const char *page_name;

  page_name = adw_preferences_window_get_visible_page_name (window);

  if (g_strcmp0 (page_name, "main") == 0)
    gtk_window_destroy (GTK_WINDOW (window));
  else
    adw_preferences_window_set_visible_page_name (window, "main");
}

/*
 * GObject
 */
static void
valent_preferences_window_constructed (GObject *object)
{
  ValentPreferencesWindow *self = VALENT_PREFERENCES_WINDOW (object);
  PeasEngine *engine = valent_get_engine ();
  const GList *plugins = NULL;

  /* Modify the dialog */
  if (!valent_preferences_window_modify (ADW_PREFERENCES_WINDOW (self)))
    g_warning ("Failed to modify AdwPreferencesWindow");

  /* Application Settings */
  self->settings = g_settings_new ("ca.andyholmes.Valent");
  g_settings_bind (self->settings,     "name",
                   self->rename_label, "label",
                   G_SETTINGS_BIND_DEFAULT);

  /* Application Plugins */
  plugins = peas_engine_get_plugin_list (engine);

  for (const GList *iter = plugins; iter; iter = iter->next)
    on_load_plugin (engine, iter->data, self);

  g_signal_connect_after (engine,
                          "load-plugin",
                          G_CALLBACK (on_load_plugin),
                          self);
  g_signal_connect (engine,
                    "unload-plugin",
                    G_CALLBACK (on_unload_plugin),
                    self);

  G_OBJECT_CLASS (valent_preferences_window_parent_class)->constructed (object);
}

static void
valent_preferences_window_dispose (GObject *object)
{
  ValentPreferencesWindow *self = VALENT_PREFERENCES_WINDOW (object);

  g_signal_handlers_disconnect_by_data (valent_get_engine (), self);
  g_clear_object (&self->settings);

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

  /* Template */
  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-preferences-window.ui");

  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, general_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, main_page);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, plugin_group);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, plugin_list);

  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, rename_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, rename_label);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, rename_dialog);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesWindow, rename_button);

  gtk_widget_class_bind_template_callback (widget_class, on_rename_dialog_open);
  gtk_widget_class_bind_template_callback (widget_class, on_rename_dialog_response);
  gtk_widget_class_bind_template_callback (widget_class, on_rename_entry_changed);

  gtk_widget_class_install_action (widget_class, "win.page", "s", page_action);
  gtk_widget_class_install_action (widget_class, "win.previous", NULL, previous_action);

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

  extensions[EXTEN_CONTACT_STORE_PROVIDER] =
    (ExtensionDescription){
      VALENT_TYPE_CONTACT_STORE_PROVIDER,
      N_("Contacts"),
      "contacts",
    };

  extensions[EXTEN_INPUT_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_INPUT_ADAPTER,
      N_("Mouse and Keyboard"),
      "input",
    };

  extensions[EXTEN_MEDIA_PLAYER_PROVIDER] =
    (ExtensionDescription){
      VALENT_TYPE_MEDIA_PLAYER_PROVIDER,
      N_("Media Players"),
      "media",
    };

  extensions[EXTEN_MIXER_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_MIXER_CONTROL,
      N_("Volume Control"),
      "mixer",
    };

  extensions[EXTEN_NOTIFICATION_ADAPTER] =
    (ExtensionDescription){
      VALENT_TYPE_NOTIFICATION_SOURCE,
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

