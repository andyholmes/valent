// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-ui.h>
#include <libvalent-contacts.h>

#include "valent-contacts-preferences.h"


struct _ValentContactsPreferences
{
  ValentDevicePreferencesPage  parent_instance;

  GHashTable                  *local_stores;

  /* Template widgets */
  AdwExpanderRow              *export_row;
  GtkSwitch                   *remote_sync;
  GtkSwitch                   *remote_import;
};

G_DEFINE_TYPE (ValentContactsPreferences, valent_contacts_preferences, VALENT_TYPE_DEVICE_PREFERENCES_PAGE)


static void
on_export_row (GtkListBox                  *box,
               GtkListBoxRow               *row,
               ValentDevicePreferencesPage *page)
{
  GSettings *settings;
  g_autofree char *local_uid = NULL;
  const char *uid;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (VALENT_IS_DEVICE_PREFERENCES_PAGE (page));

  settings = valent_device_preferences_page_get_settings (page);
  local_uid = g_settings_get_string (settings, "local-uid");
  uid = gtk_widget_get_name (GTK_WIDGET (row));

  if (g_strcmp0 (local_uid, uid) == 0)
    g_settings_reset (settings, "local-uid");
  else
    g_settings_set_string (settings, "local-uid", uid);

  gtk_list_box_invalidate_filter (box);
}

static gboolean
export_list_filter_func (GtkListBoxRow               *row,
                         ValentDevicePreferencesPage *page)
{
  GSettings *settings;
  g_autofree char *local_uid = NULL;
  const char *uid;
  GtkWidget *select;
  gboolean visible;

  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (VALENT_IS_DEVICE_PREFERENCES_PAGE (page));

  settings = valent_device_preferences_page_get_settings (page);
  local_uid = g_settings_get_string (settings, "local-uid");
  uid = gtk_widget_get_name (GTK_WIDGET (row));

  select = g_object_get_data (G_OBJECT (row), "select");
  visible = (g_strcmp0 (local_uid, uid) == 0);
  gtk_widget_set_visible (select, visible);

  return TRUE;
}

static void
on_store_selected (AdwActionRow              *row,
                   ValentContactsPreferences *self)
{
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;
  GHashTableIter iter;
  gpointer store, store_row;

  g_assert (ADW_IS_ACTION_ROW (row));
  g_assert (VALENT_IS_CONTACTS_PREFERENCES (self));

  settings = valent_device_preferences_page_get_settings (page);

  g_hash_table_iter_init (&iter, self->local_stores);

  while (g_hash_table_iter_next (&iter, &store, &store_row))
    {
      GtkWidget *check = g_object_get_data (G_OBJECT (store_row), "select");

      if (row == store_row)
        {
          const char *local_uid;

          local_uid = valent_contact_store_get_uid (store);
          g_settings_set_string (settings, "local-uid", local_uid);
        }

      gtk_widget_set_visible (check, (row == store_row));
    }
}

static void
on_store_added (ValentContacts            *contacts,
                ValentContactStore        *store,
                ValentContactsPreferences *self)
{
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;
  GtkWidget *row;
  GtkWidget *check;
  const char *icon_name;
  const char *uid;
  g_autofree char *local_uid = NULL;
  g_autofree char *device_id = NULL;

  g_assert (VALENT_IS_CONTACTS (contacts));
  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (VALENT_IS_CONTACTS_PREFERENCES (self));

  /* FIXME: select an icon name for the addressbook type */
  g_object_get (page, "device-id", &device_id, NULL);
  uid = valent_contact_store_get_uid (store);

  if (g_strcmp0 (device_id, uid) == 0)
    icon_name = APPLICATION_ID"-symbolic";
  else
    icon_name = "x-office-address-book";

  /* Row */
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "icon-name",   icon_name,
                      "title",       valent_contact_store_get_name (store),
                      NULL);

  g_signal_connect (G_OBJECT (row),
                    "activated",
                    G_CALLBACK (on_store_selected),
                    self);

  /* Check */
  settings = valent_device_preferences_page_get_settings (page);
  local_uid = g_settings_get_string (settings, "local-uid");
  check = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "icon-size", GTK_ICON_SIZE_NORMAL,
                        "visible",   (g_strcmp0 (local_uid, uid) == 0),
                        NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), check);
  g_object_set_data (G_OBJECT (row), "select", check);

  g_object_bind_property (store, "name",
                          row,   "title",
                          G_BINDING_SYNC_CREATE);

  adw_expander_row_add_row (self->export_row, row);
  g_hash_table_insert (self->local_stores, store, row);
}

static void
on_store_removed (ValentContacts            *contacts,
                  ValentContactStore        *store,
                  ValentContactsPreferences *self)
{
  gpointer row;

  if (g_hash_table_steal_extended (self->local_stores, store, NULL, &row))
    adw_expander_row_remove (self->export_row, row);
}


/*
 * GObject
 */
static void
valent_contacts_preferences_constructed (GObject *object)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (object);
  ValentDevicePreferencesPage *page = VALENT_DEVICE_PREFERENCES_PAGE (self);
  GSettings *settings;
  ValentContacts *contacts;
  g_autoptr (GPtrArray) stores = NULL;

  /* Setup GSettings */
  settings = valent_device_preferences_page_get_settings (page);

  g_settings_bind (settings,          "remote-sync",
                   self->remote_sync, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,            "remote-import",
                   self->remote_import, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,         "local-sync",
                   self->export_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);

  /* Contacts */
  contacts = valent_contacts_get_default ();
  stores = valent_contacts_get_stores (contacts);

  for (unsigned int i = 0; i < stores->len; i++)
    on_store_added (contacts, g_ptr_array_index (stores, i), self);

  g_signal_connect (contacts,
                    "store-added",
                    G_CALLBACK (on_store_added),
                    self);
  g_signal_connect (contacts,
                    "store-removed",
                    G_CALLBACK (on_store_removed),
                    self);

  G_OBJECT_CLASS (valent_contacts_preferences_parent_class)->constructed (object);
}

static void
valent_contacts_preferences_finalize (GObject *object)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (object);
  ValentContacts *contacts;

  contacts = valent_contacts_get_default ();
  g_signal_handlers_disconnect_by_func (contacts, on_store_added, self);
  g_signal_handlers_disconnect_by_func (contacts, on_store_removed, self);

  g_clear_pointer (&self->local_stores, g_hash_table_unref);

  G_OBJECT_CLASS (valent_contacts_preferences_parent_class)->finalize (object);
}

static void
valent_contacts_preferences_class_init (ValentContactsPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_contacts_preferences_constructed;
  object_class->finalize = valent_contacts_preferences_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/contacts/valent-contacts-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, export_row);
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, remote_sync);
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, remote_import);

  gtk_widget_class_bind_template_callback (widget_class, on_export_row);
}

static void
valent_contacts_preferences_init (ValentContactsPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->local_stores = g_hash_table_new (NULL, NULL);
}

