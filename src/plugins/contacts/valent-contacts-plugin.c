// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-contacts-device.h"
#include "valent-contacts-plugin.h"


struct _ValentContactsPlugin
{
  ValentDevicePlugin     parent_instance;

  GCancellable          *cancellable;

  ValentContactsAdapter *adapter;
  GListModel            *local_contacts;
};

G_DEFINE_FINAL_TYPE (ValentContactsPlugin, valent_contacts_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Contacts
 */
static void
valent_contact_plugin_handle_request_vcards_by_uid (ValentContactsPlugin *self,
                                                    JsonNode             *packet)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;
  g_autoptr (JsonNode) uids_node = NULL;
  JsonArray *uids_response = NULL;
  JsonArray *uids_request = NULL;
  GSettings *settings;
  g_autoptr (GHashTable) uidx = NULL;
  unsigned int n_uids = 0;
  unsigned int n_contacts = 0;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

#ifndef __clang_analyzer__
  /* Bail if exporting is disabled */
  settings = valent_extension_get_settings (VALENT_EXTENSION (self));

  if (self->local_contacts == NULL || !g_settings_get_boolean (settings, "local-sync"))
    return;

  if (!valent_packet_get_array (packet, "uids", &uids_request))
    {
      g_debug ("%s(): expected \"uids\" field holding an array",
               G_STRFUNC);
      return;
    }

  uidx = g_hash_table_new (g_str_hash, g_str_equal);
  n_uids = json_array_get_length (uids_request);
  for (unsigned int i = 0; i < n_uids; i++)
    {
      const char *uid = json_array_get_string_element (uids_request, i);
      if (uid != NULL && *uid != '\0')
        g_hash_table_add (uidx, (char *)uid);
    }

  if (g_hash_table_size (uidx) == 0)
    return;

  valent_packet_init (&builder, "kdeconnect.contacts.response_vcards");
  uids_response = json_array_new ();

  n_contacts = g_list_model_get_n_items (self->local_contacts);
  for (unsigned int i = 0; i < n_contacts; i++)
    {
      g_autoptr (EContact) contact = NULL;
      const char *uid = NULL;

      contact = g_list_model_get_item (self->local_contacts, i);
      uid = e_contact_get_const (contact, E_CONTACT_UID);
      if (g_hash_table_contains (uidx, uid))
        {
          g_autofree char *vcard_data = NULL;

          vcard_data = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_21);

          json_builder_set_member_name (builder, uid);
          json_builder_add_string_value (builder, vcard_data);

          json_array_add_string_element (uids_response, uid);
        }
    }

  uids_node = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (uids_node, g_steal_pointer (&uids_response));
  json_builder_set_member_name (builder, "uids");
  json_builder_add_value (builder, g_steal_pointer (&uids_node));

  response = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
#endif /* __clang_analyzer__ */
}

static void
valent_contact_plugin_handle_request_all_uids_timestamps (ValentContactsPlugin *self,
                                                          JsonNode             *packet)
{
  GSettings *settings;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;
  g_autoptr (JsonNode) uids_node = NULL;
  JsonArray *uids_response = NULL;
  unsigned int n_items = 0;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  settings = valent_extension_get_settings (VALENT_EXTENSION (self));
  if (self->local_contacts == NULL || !g_settings_get_boolean (settings, "local-sync"))
    return;

  valent_packet_init (&builder, "kdeconnect.contacts.response_uids_timestamps");
  uids_response = json_array_new ();

  n_items = g_list_model_get_n_items (self->local_contacts);
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (EContact) contact = NULL;
      const char *uid;
      int64_t timestamp = 0;

      contact = g_list_model_get_item (self->local_contacts, i);
      uid = e_contact_get_const (contact, E_CONTACT_UID);
      json_builder_set_member_name (builder, uid);

      // TODO: We probably need to convert between the custom field
      // `X-KDECONNECT-TIMESTAMP` and `E_CONTACT_REV` to set a proper timestamp
      timestamp = 0;
      json_builder_add_int_value (builder, timestamp);
      json_array_add_string_element (uids_response, uid);
    }

  uids_node = json_node_new (JSON_NODE_ARRAY);
  json_node_take_array (uids_node, g_steal_pointer (&uids_response));
  json_builder_set_member_name (builder, "uids");
  json_builder_add_value (builder, g_steal_pointer (&uids_node));

  response = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

/*
 * GActions
 */
static void
contacts_fetch_action (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (user_data);
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  packet = valent_packet_new ("kdeconnect.contacts.request_all_uids_timestamps");
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static const GActionEntry actions[] = {
    {"fetch", contacts_fetch_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_contacts_plugin_update_state (ValentDevicePlugin *plugin,
                                     ValentDeviceState   state)
{
  gboolean available;

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);
}

static void
valent_contacts_plugin_handle_packet (ValentDevicePlugin *plugin,
                                      const char         *type,
                                      JsonNode           *packet)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (plugin);

  g_assert (VALENT_IS_CONTACTS_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  /* A response to a request for a listing of contacts or vCards
   */
  if (g_str_equal (type, "kdeconnect.contacts.response_uids_timestamps") ||
      g_str_equal (type, "kdeconnect.contacts.response_vcards"))
    valent_contacts_device_handle_packet (self->adapter, type, packet);

  /* A request for a listing of contacts
   */
  else if (g_str_equal (type, "kdeconnect.contacts.request_all_uids_timestamps"))
    valent_contact_plugin_handle_request_all_uids_timestamps (self, packet);

  /* A request for contacts
   */
  else if (g_str_equal (type, "kdeconnect.contacts.request_vcards_by_uid"))
    valent_contact_plugin_handle_request_vcards_by_uid (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_contacts_plugin_destroy (ValentObject *object)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->adapter != NULL)
    {
      valent_contacts_unexport_adapter (valent_contacts_get_default (),
                                        self->adapter);
      g_clear_object (&self->adapter);
    }

  g_clear_object (&self->local_contacts);

  VALENT_OBJECT_CLASS (valent_contacts_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_contacts_plugin_constructed (GObject *object)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);
  ValentDevice *device = NULL;
  ValentContacts *contacts = valent_contacts_get_default ();
  g_autofree char *local_iri = NULL;
  GSettings *settings;

  G_OBJECT_CLASS (valent_contacts_plugin_parent_class)->constructed (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  self->cancellable = g_cancellable_new ();

  device = valent_extension_get_object (VALENT_EXTENSION (plugin));
  settings = valent_extension_get_settings (VALENT_EXTENSION (self));

  /* Remote Adapter
   */
  self->adapter = valent_contacts_device_new (device);
  valent_contacts_export_adapter (valent_contacts_get_default (),
                                  self->adapter);

  /* Local address book, shared with remote device
   */
  local_iri = g_settings_get_string (settings, "local-uid");
  if (local_iri != NULL && *local_iri != '\0')
    {
      unsigned int n_adapters = g_list_model_get_n_items (G_LIST_MODEL (contacts));

      for (unsigned int i = 0; i < n_adapters; i++)
        {
          g_autoptr (GListModel) adapter = NULL;
          g_autofree char *iri = NULL;

          adapter = g_list_model_get_item (G_LIST_MODEL (contacts), i);
          iri = valent_object_dup_iri (VALENT_OBJECT (adapter));
          if (g_strcmp0 (local_iri, iri) == 0)
            {
              self->local_contacts = g_list_model_get_item (adapter, i);
              break;
            }
        }
    }
}

static void
valent_contacts_plugin_class_init (ValentContactsPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_contacts_plugin_constructed;

  vobject_class->destroy = valent_contacts_plugin_destroy;

  plugin_class->handle_packet = valent_contacts_plugin_handle_packet;
  plugin_class->update_state = valent_contacts_plugin_update_state;
}

static void
valent_contacts_plugin_init (ValentContactsPlugin *self)
{
}

