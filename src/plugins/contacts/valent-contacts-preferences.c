// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>
#include <libvalent-contacts.h>

#include "valent-contacts-preferences.h"


struct _ValentContactsPreferences
{
  AdwPreferencesPage  parent_instance;

  GSettings          *settings;
  char               *plugin_context;

  GHashTable         *local_stores;

  /* Template widgets */
  AdwExpanderRow     *export_row;
  GtkSwitch          *remote_sync;
  GtkSwitch          *remote_import;
};

/* Interfaces */
static void valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentContactsPreferences, valent_contacts_preferences, ADW_TYPE_PREFERENCES_PAGE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_PLUGIN_PREFERENCES, valent_plugin_preferences_iface_init))


enum {
  PROP_0,
  PROP_PLUGIN_CONTEXT,
  N_PROPERTIES
};


static void
on_export_row (GtkListBox                *box,
               GtkListBoxRow             *row,
               ValentContactsPreferences *self)
{
  g_autofree char *local_uid = NULL;
  const char *uid;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));

  local_uid = g_settings_get_string (self->settings, "local-uid");
  uid = gtk_widget_get_name (GTK_WIDGET (row));

  if (g_strcmp0 (local_uid, uid) == 0)
    g_settings_reset (self->settings, "local-uid");
  else
    g_settings_set_string (self->settings, "local-uid", uid);

  gtk_list_box_invalidate_filter (box);
}

static gboolean
export_list_filter_func (GtkListBoxRow             *row,
                         ValentContactsPreferences *self)
{
  g_autofree char *local_uid = NULL;
  const char *uid;
  GtkWidget *select;
  gboolean visible;

  uid = gtk_widget_get_name (GTK_WIDGET (row));
  local_uid = g_settings_get_string (self->settings, "local-uid");

  select = g_object_get_data (G_OBJECT (row), "select");
  visible = (g_strcmp0 (local_uid, uid) == 0);
  gtk_widget_set_visible (select, visible);

  return TRUE;
}

static void
on_store_selected (AdwActionRow              *row,
                   ValentContactsPreferences *self)
{
  GHashTableIter iter;
  gpointer store, store_row;

  g_hash_table_iter_init (&iter, self->local_stores);

  while (g_hash_table_iter_next (&iter, &store, &store_row))
    {
      GtkWidget *check = g_object_get_data (G_OBJECT (store_row), "select");

      if (row == store_row)
        {
          const char *local_uid;

          local_uid = valent_contact_store_get_uid (store);
          g_settings_set_string (self->settings, "local-uid", local_uid);
        }

      gtk_widget_set_visible (check, (row == store_row));
    }
}

static void
on_store_added (ValentContacts            *contacts,
                ValentContactStore        *store,
                ValentContactsPreferences *self)
{
  GtkWidget *row;
  GtkWidget *check;
  const char *icon_name;
  const char *uid;
  g_autofree char *local_uid = NULL;

  /* FIXME: select an icon name for the addressbook type */
  uid = valent_contact_store_get_uid (store);

  if (g_strcmp0 (self->plugin_context, uid) == 0)
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
  local_uid = g_settings_get_string (self->settings, "local-uid");
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

  adw_expander_row_add (self->export_row, row);
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
 * ValentPluginPreferences
 */
static void
valent_plugin_preferences_iface_init (ValentPluginPreferencesInterface *iface)
{
}


/*
 * GObject
 */
static void
valent_contacts_preferences_constructed (GObject *object)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (object);
  ValentContacts *contacts;
  g_autoptr (GPtrArray) stores = NULL;

  /* Setup GSettings */
  self->settings = valent_device_plugin_new_settings (self->plugin_context,
                                                      "contacts");

  g_settings_bind (self->settings,    "remote-sync",
                   self->remote_sync, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings,      "remote-import",
                   self->remote_import, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings,   "local-sync",
                   self->export_row, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);

  /* Contacts */
  contacts = valent_contacts_get_default ();
  stores = valent_contacts_get_stores (contacts);

  for (guint i = 0; i < stores->len; i++)
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
  g_clear_pointer (&self->plugin_context, g_free);
  g_clear_object (&self->settings);

  G_OBJECT_CLASS (valent_contacts_preferences_parent_class)->finalize (object);
}

static void
valent_contacts_preferences_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      g_value_set_string (value, self->plugin_context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contacts_preferences_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (object);

  switch (prop_id)
    {
    case PROP_PLUGIN_CONTEXT:
      self->plugin_context = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contacts_preferences_class_init (ValentContactsPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_contacts_preferences_constructed;
  object_class->finalize = valent_contacts_preferences_finalize;
  object_class->get_property = valent_contacts_preferences_get_property;
  object_class->set_property = valent_contacts_preferences_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/contacts/valent-contacts-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, export_row);
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, remote_sync);
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, remote_import);

  gtk_widget_class_bind_template_callback (widget_class, on_export_row);

  g_object_class_override_property (object_class,
                                    PROP_PLUGIN_CONTEXT,
                                    "plugin-context");
}

static void
valent_contacts_preferences_init (ValentContactsPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->local_stores = g_hash_table_new (NULL, NULL);
}

