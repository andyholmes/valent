// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-device-preferences-dialog"

#include "config.h"

#include <glib/gi18n-lib.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-preferences-sync-page.h"


struct _ValentPreferencesSyncPage
{
  ValentPreferencesPage  parent_instance;

  GHashTable            *local_stores;

  /* template */
  GtkSwitch             *sync_pull;
  GtkSwitch             *sync_push;

  AdwExpanderRow        *local_sync;
  GtkListBox            *local_list;
  GtkSwitch             *remote_sync;

  AdwExpanderRow        *forward_notifications;
  GtkSwitch             *forward_when_active;
  AdwNavigationPage     *application_page;
  GtkStack              *application_title_stack;
  GtkToggleButton       *application_filter_button;
  GtkSearchEntry        *application_filter_entry;
  AdwActionRow          *application_row;
  GtkListBox            *application_list;
  char                  *application_filter;
  GHashTable            *application_rows;

  GtkSwitch             *auto_mount;
  AdwExpanderRow        *local_allow;
  GtkAdjustment         *local_port;
};

G_DEFINE_FINAL_TYPE (ValentPreferencesSyncPage, valent_preferences_sync_page, VALENT_TYPE_PREFERENCES_PAGE)

/*
 * Contacts
 */
static void
on_local_sync (GtkListBox            *box,
               GtkListBoxRow         *row,
               ValentPreferencesPage *page)
{
  GSettings *settings;
  g_autofree char *local_uid = NULL;
  const char *uid;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (VALENT_IS_PREFERENCES_PAGE (page));

  settings = valent_preferences_page_get_settings (page, "contacts");
  local_uid = g_settings_get_string (settings, "local-uid");
  uid = gtk_widget_get_name (GTK_WIDGET (row));

  if (g_strcmp0 (local_uid, uid) == 0)
    g_settings_reset (settings, "local-uid");
  else
    g_settings_set_string (settings, "local-uid", uid);

  gtk_list_box_invalidate_filter (box);
}

static void
on_adapter_selected (AdwActionRow          *row,
                     ValentPreferencesPage *page)
{
  ValentPreferencesSyncPage *self = VALENT_PREFERENCES_SYNC_PAGE (page);
  GSettings *settings;
  GHashTableIter iter;
  ValentContactsAdapter *adapter;
  gpointer store_row;

  g_assert (ADW_IS_ACTION_ROW (row));
  g_assert (VALENT_IS_PREFERENCES_PAGE (page));

  settings = valent_preferences_page_get_settings (page, "contacts");

  g_hash_table_iter_init (&iter, self->local_stores);
  while (g_hash_table_iter_next (&iter, (void **)&adapter, &store_row))
    {
      GtkWidget *check = g_object_get_data (G_OBJECT (store_row), "select");

      if (row == store_row)
        {
          const char *iri = valent_resource_get_iri (VALENT_RESOURCE (self));
          g_settings_set_string (settings, "local-uid", iri != NULL ? iri : "");
        }

      gtk_widget_set_visible (check, (row == store_row));
    }
}

static GtkWidget *
valent_contacts_preferences_create_row_func (gpointer item,
                                             gpointer user_data)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (user_data);
  ValentPreferencesSyncPage *self = VALENT_PREFERENCES_SYNC_PAGE (user_data);
  ValentContactsAdapter *adapter = VALENT_CONTACTS_ADAPTER (item);
  GSettings *settings;
  GtkWidget *row;
  GtkWidget *image;
  const char *iri = NULL;
  g_autofree char *local_iri = NULL;

  g_assert (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));

  iri = valent_resource_get_iri (VALENT_RESOURCE (adapter));
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "title",       iri,
                      NULL);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "x-office-address-book",
                        "icon-size", GTK_ICON_SIZE_NORMAL,
                        NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  g_signal_connect_object (G_OBJECT (row),
                           "activated",
                           G_CALLBACK (on_adapter_selected),
                           self,
                           G_CONNECT_DEFAULT);

  settings = valent_preferences_page_get_settings (page, "contacts");
  local_iri = g_settings_get_string (settings, "local-uid");
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "icon-size", GTK_ICON_SIZE_NORMAL,
                        "visible",   (g_strcmp0 (local_iri, iri) == 0),
                        NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), image);
  g_object_set_data (G_OBJECT (row), "select", image);

  return row;
}

static GtkListBox *
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
 * Notifications
 */
static gboolean
application_list_filter (GtkListBoxRow *row,
                         gpointer       user_data)
{
  ValentPreferencesSyncPage *self = VALENT_PREFERENCES_SYNC_PAGE (user_data);
  g_autofree char *haystack = NULL;
  const char *title = NULL;

  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));

  if (self->application_filter == NULL || *self->application_filter == '\0')
    return TRUE;

  title = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  haystack = g_utf8_casefold (title, -1);

  return g_strrstr (haystack, self->application_filter) != NULL;
}

static int
application_list_sort (GtkListBoxRow *row1,
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
 * Template Callbacks
 */
static void
on_search_changed (GtkSearchEntry            *entry,
                   ValentPreferencesSyncPage *self)
{
  g_autofree char *query = NULL;

  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));

  query = g_utf8_casefold (gtk_editable_get_text (GTK_EDITABLE (entry)), -1);

  if (g_set_str (&self->application_filter, query))
    gtk_list_box_invalidate_filter (self->application_list);
}

static void
on_search_toggled (GtkToggleButton           *button,
                   GParamSpec                *pspec,
                   ValentPreferencesSyncPage *self)
{
  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));

  if (gtk_toggle_button_get_active (button))
    {
      gtk_stack_set_visible_child_name (self->application_title_stack, "search");
      gtk_widget_grab_focus (GTK_WIDGET (self->application_filter_entry));
      gtk_editable_set_position (GTK_EDITABLE (self->application_filter_entry), -1);
    }
  else
    {
      gtk_editable_set_text (GTK_EDITABLE (self->application_filter_entry), "");
      gtk_stack_set_visible_child_name (self->application_title_stack, "title");
    }
}

static void
on_search_started (GtkSearchEntry            *entry,
                   ValentPreferencesSyncPage *self)
{
  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));

  gtk_toggle_button_set_active (self->application_filter_button, TRUE);
}

static void
on_stop_search (GtkSearchEntry            *entry,
                ValentPreferencesSyncPage *self)
{
  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));

  gtk_toggle_button_set_active (self->application_filter_button, FALSE);
}

static void
on_switch_toggled (GObject                   *object,
                   GParamSpec                *pspec,
                   ValentPreferencesSyncPage *self)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  GSettings *settings;
  GHashTableIter iter;
  gpointer row;
  GVariantBuilder builder;
  GVariant *forward_deny;

  g_hash_table_iter_init (&iter, self->application_rows);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_STRING_ARRAY);

  while (g_hash_table_iter_next (&iter, &row, NULL))
    {
      const char *name;

      if (adw_switch_row_get_active (ADW_SWITCH_ROW (row)))
        continue;

      name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
      g_variant_builder_add (&builder, "s", name);
    }

  forward_deny = g_variant_builder_end (&builder);
  settings = valent_preferences_page_get_settings (page, "notification");
  g_settings_set_value (settings, "forward-deny", forward_deny);
}

static void
add_application (ValentPreferencesSyncPage *self,
                 GVariant                  *app,
                 gboolean                   enabled)
{
  GtkWidget *row;
  GtkWidget *image;
  const char *title;
  g_autoptr (GVariant) icon_v = NULL;
  g_autoptr (GIcon) icon = NULL;

  if (!g_variant_lookup (app, "name", "&s", &title))
    return;

  row = g_object_new (ADW_TYPE_SWITCH_ROW,
                      "activatable", TRUE,
                      "selectable",  TRUE,
                      "title",       title,
                      "active",      enabled,
                      NULL);
  g_signal_connect_object (row,
                           "notify::active",
                           G_CALLBACK (on_switch_toggled),
                           self,
                           G_CONNECT_DEFAULT);

  /* App icon */
  if ((icon_v = g_variant_lookup_value (app, "icon", NULL)) != NULL)
    icon = g_icon_deserialize (icon_v);

  if (icon == NULL)
    icon = g_icon_new_for_string ("application-x-executable", NULL);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "gicon",     icon,
                        "icon-size", GTK_ICON_SIZE_LARGE,
                        NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  g_hash_table_add (self->application_rows, row);
  gtk_list_box_insert (self->application_list, row, -1);
}

static void
populate_applications (ValentPreferencesSyncPage *self)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  GSettings *settings;
  GVariant *known;
  g_auto (GStrv) deny = NULL;
  GVariantIter iter;
  const char *key;
  GVariant *value;

  settings = valent_preferences_page_get_settings (page, "notification");
  deny = g_settings_get_strv (settings, "forward-deny");

  /* Query the known applications */
  known = valent_notifications_get_applications (NULL);
  g_variant_iter_init (&iter, known);

  while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
    {
      gboolean enabled;

      enabled = !g_strv_contains ((const char * const *)deny, key);
      add_application (self, value, enabled);
    }
}

/*
 * GAction
 */
static void
applications_action (GtkWidget  *widget,
                     const char *action,
                     GVariant   *parameter)
{
  ValentPreferencesSyncPage *self = VALENT_PREFERENCES_SYNC_PAGE (widget);
  GtkWidget *dialog = NULL;

  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));

  dialog = gtk_widget_get_ancestor (widget, ADW_TYPE_PREFERENCES_DIALOG);
  if (dialog != NULL)
    {
      adw_preferences_dialog_push_subpage (ADW_PREFERENCES_DIALOG (dialog),
                                           self->application_page);
    }
}

static void
reset_action (GtkWidget  *widget,
              const char *action,
              GVariant   *parameter)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (widget);
  ValentPreferencesSyncPage *self = VALENT_PREFERENCES_SYNC_PAGE (widget);
  GSettings *settings;
  GHashTableIter iter;
  gpointer row_switch;

  g_assert (VALENT_IS_PREFERENCES_SYNC_PAGE (self));
  g_assert (VALENT_IS_PREFERENCES_PAGE (self));

  g_hash_table_iter_init (&iter, self->application_rows);

  while (g_hash_table_iter_next (&iter, NULL, &row_switch))
    gtk_switch_set_active (GTK_SWITCH (row_switch), TRUE);

  settings = valent_preferences_page_get_settings (page, "notification");
  g_settings_reset (settings, "applications");
}

/*
 * SFTP
 */
static void
on_toggle_row (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data)
{
  gboolean active;
  GtkWidget *grid;
  GtkWidget *toggle;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  grid = gtk_list_box_row_get_child (row);
  toggle = gtk_grid_get_child_at (GTK_GRID (grid), 1, 0);

  g_object_get (toggle, "active", &active, NULL);
  g_object_set (toggle, "active", !active, NULL);
}

/*
 * ValentPreferencesPage
 */
static inline void
valent_preferences_sync_page_bind_context (ValentPreferencesSyncPage *self)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (self);
  GSettings *settings = NULL;

  /* Clipboard
   */
  settings = valent_preferences_page_get_settings (page, "clipboard");
  g_settings_bind (settings,        "auto-pull",
                   self->sync_pull, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,        "auto-push",
                   self->sync_push, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* Contacts
   */
  settings = valent_preferences_page_get_settings (page, "contacts");
  g_settings_bind (settings,          "remote-sync",
                   self->remote_sync, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,         "local-sync",
                   self->local_sync, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);

  /* Notifications
   */
  settings = valent_preferences_page_get_settings (page, "notification");
  g_settings_bind (settings,                    "forward-notifications",
                   self->forward_notifications, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,                    "forward-when-active",
                   self->forward_when_active,   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  populate_applications (self);

  /* SFTP
   */
  settings = valent_preferences_page_get_settings (page, "sftp");
  g_settings_bind (settings,         "auto-mount",
                   self->auto_mount, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,          "local-allow",
                   self->local_allow, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,         "local-port",
                   self->local_port, "value",
                   G_SETTINGS_BIND_DEFAULT);
}

/*
 * GObject
 */
static void
valent_preferences_sync_page_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_PREFERENCES_SYNC_PAGE);

  G_OBJECT_CLASS (valent_preferences_sync_page_parent_class)->dispose (object);
}

static void
valent_preferences_sync_page_finalize (GObject *object)
{
  ValentPreferencesSyncPage *self = VALENT_PREFERENCES_SYNC_PAGE (object);

  g_clear_pointer (&self->application_filter, g_free);
  g_clear_pointer (&self->application_rows, g_hash_table_unref);
  g_clear_pointer (&self->local_stores, g_hash_table_unref);

  G_OBJECT_CLASS (valent_preferences_sync_page_parent_class)->finalize (object);
}

static void
valent_preferences_sync_page_notify (GObject    *object,
                                     GParamSpec *pspec)
{
  ValentPreferencesPage *page = VALENT_PREFERENCES_PAGE (object);
  ValentPreferencesSyncPage *self = VALENT_PREFERENCES_SYNC_PAGE (object);

  if (g_strcmp0 (pspec->name, "context") == 0)
    {
      if (valent_preferences_page_get_context (page) != NULL)
        valent_preferences_sync_page_bind_context (self);
    }

  if (G_OBJECT_CLASS (valent_preferences_sync_page_parent_class)->notify)
    G_OBJECT_CLASS (valent_preferences_sync_page_parent_class)->notify (object,
                                                                        pspec);
}

static void
valent_preferences_sync_page_class_init (ValentPreferencesSyncPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_preferences_sync_page_dispose;
  object_class->finalize = valent_preferences_sync_page_finalize;
  object_class->notify = valent_preferences_sync_page_notify;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-preferences-sync-page.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, sync_pull);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, sync_push);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, local_sync);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, remote_sync);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, forward_notifications);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, forward_when_active);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, application_page);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, application_title_stack);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, application_filter_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, application_filter_button);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, application_list);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, application_row);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, auto_mount);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, local_allow);
  gtk_widget_class_bind_template_child (widget_class, ValentPreferencesSyncPage, local_port);
  gtk_widget_class_bind_template_callback (widget_class, on_local_sync);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_started);
  gtk_widget_class_bind_template_callback (widget_class, on_search_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_stop_search);
  gtk_widget_class_bind_template_callback (widget_class, on_toggle_row);

  gtk_widget_class_install_action (widget_class, "preferences.applications", NULL, applications_action);
  gtk_widget_class_install_action (widget_class, "preferences.reset", NULL, reset_action);

}

static void
valent_preferences_sync_page_init (ValentPreferencesSyncPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Contacts
   */
  self->local_stores = g_hash_table_new (NULL, NULL);
  self->local_list = _adw_expander_row_get_list (self->local_sync);
  gtk_list_box_bind_model (self->local_list,
                           G_LIST_MODEL (valent_contacts_get_default ()),
                           valent_contacts_preferences_create_row_func,
                           self, NULL);

  /* Notifications
   */
  self->application_rows = g_hash_table_new (NULL, NULL);
  gtk_search_entry_set_key_capture_widget (self->application_filter_entry,
                                           GTK_WIDGET (self->application_page));
  gtk_list_box_set_filter_func (self->application_list,
                                application_list_filter,
                                self, NULL);
  gtk_list_box_set_sort_func (self->application_list,
                              application_list_sort,
                              self, NULL);
}

