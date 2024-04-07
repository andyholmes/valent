// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-preferences"

#include "config.h"

#include <glib/gi18n.h>
#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-contacts-preferences.h"


struct _ValentContactsPreferences
{
  ValentDevicePreferencesGroup  parent_instance;

  GHashTable                   *local_stores;

  /* template */
  AdwExpanderRow               *local_sync;
  GtkListBox                   *local_list;
  GtkSwitch                    *remote_sync;
};

G_DEFINE_FINAL_TYPE (ValentContactsPreferences, valent_contacts_preferences, VALENT_TYPE_DEVICE_PREFERENCES_GROUP)


static void
on_local_sync (GtkListBox                   *box,
               GtkListBoxRow                *row,
               ValentDevicePreferencesGroup *group)
{
  GSettings *settings;
  g_autofree char *local_uid = NULL;
  const char *uid;

  g_assert (GTK_IS_LIST_BOX (box));
  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (VALENT_IS_DEVICE_PREFERENCES_GROUP (group));

  settings = valent_device_preferences_group_get_settings (group);
  local_uid = g_settings_get_string (settings, "local-uid");
  uid = gtk_widget_get_name (GTK_WIDGET (row));

  if (g_strcmp0 (local_uid, uid) == 0)
    g_settings_reset (settings, "local-uid");
  else
    g_settings_set_string (settings, "local-uid", uid);

  gtk_list_box_invalidate_filter (box);
}

static void
on_store_selected (AdwActionRow              *row,
                   ValentContactsPreferences *self)
{
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  GHashTableIter iter;
  gpointer store, store_row;

  g_assert (ADW_IS_ACTION_ROW (row));
  g_assert (VALENT_IS_CONTACTS_PREFERENCES (self));

  settings = valent_device_preferences_group_get_settings (group);

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

static GtkWidget *
valent_contacts_preferences_create_row_func (gpointer item,
                                             gpointer user_data)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (user_data);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  ValentContactStore *store = VALENT_CONTACT_STORE (item);
  ValentContext *context = NULL;
  const char *device_id = NULL;
  GSettings *settings;
  GtkWidget *row;
  GtkWidget *image;
  const char *icon_name;
  const char *uid;
  g_autofree char *local_uid = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (VALENT_IS_CONTACTS_PREFERENCES (self));

  /* FIXME: select an icon name for the addressbook type */
  context = valent_device_preferences_group_get_context (group);
  device_id = valent_context_get_id (context);
  uid = valent_contact_store_get_uid (store);

  if (g_strcmp0 (device_id, uid) == 0)
    icon_name = APPLICATION_ID"-symbolic";
  else
    icon_name = "x-office-address-book";

  /* Row */
  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "activatable", TRUE,
                      "title",       valent_contact_store_get_name (store),
                      NULL);
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", icon_name,
                        "icon-size", GTK_ICON_SIZE_NORMAL,
                        NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  g_signal_connect_object (G_OBJECT (row),
                           "activated",
                           G_CALLBACK (on_store_selected),
                           self, 0);

  /* Check */
  settings = valent_device_preferences_group_get_settings (group);
  local_uid = g_settings_get_string (settings, "local-uid");
  image = g_object_new (GTK_TYPE_IMAGE,
                        "icon-name", "object-select-symbolic",
                        "icon-size", GTK_ICON_SIZE_NORMAL,
                        "visible",   (g_strcmp0 (local_uid, uid) == 0),
                        NULL);
  adw_action_row_add_suffix (ADW_ACTION_ROW (row), image);
  g_object_set_data (G_OBJECT (row), "select", image);

  g_object_bind_property (store, "name",
                          row,   "title",
                          G_BINDING_SYNC_CREATE);

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
 * GObject
 */
static void
valent_contacts_preferences_constructed (GObject *object)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (object);
  ValentDevicePreferencesGroup *group = VALENT_DEVICE_PREFERENCES_GROUP (self);
  GSettings *settings;
  ValentContacts *contacts;

  settings = valent_device_preferences_group_get_settings (group);
  g_settings_bind (settings,          "remote-sync",
                   self->remote_sync, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (settings,         "local-sync",
                   self->local_sync, "enable-expansion",
                   G_SETTINGS_BIND_DEFAULT);

  contacts = valent_contacts_get_default ();
  gtk_list_box_bind_model (self->local_list,
                           G_LIST_MODEL (contacts),
                           valent_contacts_preferences_create_row_func,
                           self, NULL);

  G_OBJECT_CLASS (valent_contacts_preferences_parent_class)->constructed (object);
}

static void
valent_contacts_preferences_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_CONTACTS_PREFERENCES);

  G_OBJECT_CLASS (valent_contacts_preferences_parent_class)->dispose (object);
}

static void
valent_contacts_preferences_finalize (GObject *object)
{
  ValentContactsPreferences *self = VALENT_CONTACTS_PREFERENCES (object);

  g_clear_pointer (&self->local_stores, g_hash_table_unref);

  G_OBJECT_CLASS (valent_contacts_preferences_parent_class)->finalize (object);
}

static void
valent_contacts_preferences_class_init (ValentContactsPreferencesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_contacts_preferences_constructed;
  object_class->dispose = valent_contacts_preferences_dispose;
  object_class->finalize = valent_contacts_preferences_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/contacts/valent-contacts-preferences.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, local_sync);
  gtk_widget_class_bind_template_child (widget_class, ValentContactsPreferences, remote_sync);

  gtk_widget_class_bind_template_callback (widget_class, on_local_sync);
}

static void
valent_contacts_preferences_init (ValentContactsPreferences *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->local_list = _adw_expander_row_get_list (self->local_sync);
  self->local_stores = g_hash_table_new (NULL, NULL);
}

