// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-plugin"

#include "config.h"

#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-contacts-plugin.h"


struct _ValentContactsPlugin
{
  ValentDevicePlugin  parent_instance;

  GCancellable       *cancellable;

  ValentContactStore *local_store;
  ValentContactStore *remote_store;
};

G_DEFINE_FINAL_TYPE (ValentContactsPlugin, valent_contacts_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/*
 * Local Contacts
 */
static void
valent_contact_store_query_vcards_cb (ValentContactStore   *store,
                                      GAsyncResult         *result,
                                      ValentContactsPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;
  g_autoslist (GObject) contacts = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));

  contacts = valent_contact_store_query_finish (store, result, &error);

  if (error != NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  valent_packet_init (&builder, "kdeconnect.contacts.response_vcards");

  /* Add UIDs */
  json_builder_set_member_name (builder, "uids");
  json_builder_begin_array (builder);

  for (const GSList *iter = contacts; iter; iter = iter->next)
    {
      const char *uid;

      uid = e_contact_get_const (iter->data, E_CONTACT_UID);
      json_builder_add_string_value (builder, uid);
    }
  json_builder_end_array (builder);

  /* Add vCard data */
  for (const GSList *iter = contacts; iter; iter = iter->next)
    {
      const char *uid;
      g_autofree char *vcard_data = NULL;

      uid = e_contact_get_const (iter->data, E_CONTACT_UID);
      json_builder_set_member_name (builder, uid);

      vcard_data = e_vcard_to_string (iter->data, EVC_FORMAT_VCARD_21);
      json_builder_add_string_value (builder, vcard_data);
    }

  /* Finish and send the response */
  response = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

static void
valent_contact_plugin_handle_request_vcards_by_uid (ValentContactsPlugin *self,
                                                    JsonNode             *packet)
{
  GSettings *settings;
  g_autofree EBookQuery **queries = NULL;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;
  JsonArray *uids;
  unsigned int n_uids, n_queries;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

#ifndef __clang_analyzer__
  /* Bail if exporting is disabled */
  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  if (self->local_store == NULL || !g_settings_get_boolean (settings, "local-sync"))
    return;

  if (!valent_packet_get_array (packet, "uids", &uids))
    {
      g_debug ("%s(): expected \"uids\" field holding an array",
               G_STRFUNC);
      return;
    }

  /* Build a list of queries */
  n_uids = json_array_get_length (uids);
  queries = g_new (EBookQuery *, n_uids);
  n_queries = 0;

  for (unsigned int i = 0; i < n_uids; i++)
    {
      JsonNode *element = json_array_get_element (uids, i);
      const char *uid = NULL;

      if G_LIKELY (json_node_get_value_type (element) == G_TYPE_STRING)
        uid = json_node_get_string (element);

      if G_UNLIKELY (uid == NULL || *uid == '\0')
        {
          g_debug ("%s(): expected \"uids\" element to contain a string",
                   G_STRFUNC);
          continue;
        }

      queries[n_queries++] = e_book_query_field_test (E_CONTACT_UID,
                                                      E_BOOK_QUERY_IS,
                                                      uid);
    }

  if (n_queries == 0)
    return;

  query = e_book_query_or (n_queries, queries, TRUE);
  sexp = e_book_query_to_string (query);

  valent_contact_store_query (self->local_store,
                              sexp,
                              self->cancellable,
                              (GAsyncReadyCallback)valent_contact_store_query_vcards_cb,
                              self);
#endif /* __clang_analyzer__ */
}

static void
valent_contact_store_query_uids_cb (ValentContactStore   *store,
                                    GAsyncResult         *result,
                                    ValentContactsPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;
  g_autoslist (GObject) contacts = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));

  contacts = valent_contact_store_query_finish (store, result, &error);

  /* If the operation was cancelled, we're about to dispose. For any other
   * error log a warning and send an empty list. */
  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    return;

  if (error != NULL)
    g_warning ("%s(): %s", G_STRFUNC, error->message);

  /* Build response */
  valent_packet_init (&builder, "kdeconnect.contacts.response_uids_timestamps");

  for (const GSList *iter = contacts; iter; iter = iter->next)
    {
      const char *uid;
      int64_t timestamp = 0;

      uid = e_contact_get_const (iter->data, E_CONTACT_UID);
      json_builder_set_member_name (builder, uid);

      // TODO: We probably need to convert between the custom field
      // `X-KDECONNECT-TIMESTAMP` and `E_CONTACT_REV` to set a proper timestamp
      timestamp = 0;
      json_builder_add_int_value (builder, timestamp);
    }

  response = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

static void
valent_contact_plugin_handle_request_all_uids_timestamps (ValentContactsPlugin *self,
                                                          JsonNode             *packet)
{
  GSettings *settings;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  /* Bail if exporting is disabled */
  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  if (self->local_store == NULL || !g_settings_get_boolean (settings, "local-sync"))
    return;

  query = e_book_query_vcard_field_exists (EVC_UID);
  sexp = e_book_query_to_string (query);

  valent_contact_store_query (self->local_store,
                              sexp,
                              self->cancellable,
                              (GAsyncReadyCallback)valent_contact_store_query_uids_cb,
                              self);
}

/*
 * Remote Contacts
 */
static void
valent_contact_plugin_handle_response_uids_timestamps (ValentContactsPlugin *self,
                                                       JsonNode             *packet)
{
  g_autoptr (JsonNode) request = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  JsonObjectIter iter;
  const char *uid;
  JsonNode *node;
  unsigned int n_requested = 0;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  /* Start the packet */
  valent_packet_init (&builder, "kdeconnect.contacts.request_vcards_by_uid");
  json_builder_set_member_name (builder, "uids");
  json_builder_begin_array (builder);

  json_object_iter_init (&iter, valent_packet_get_body (packet));

  while (json_object_iter_next (&iter, &uid, &node))
    {
      int64_t timestamp = 0;

      // skip the "uids" array
      if G_UNLIKELY (g_str_equal ("uids", uid))
        continue;

      if G_LIKELY (json_node_get_value_type (node) == G_TYPE_INT64)
        timestamp = json_node_get_int (node);

      /* Check if the contact is new or updated */
      if (0 != timestamp)
      //if (valent_contact_store_get_timestamp (self->store, uid) != timestamp)
        {
          n_requested++;
          json_builder_add_string_value (builder, uid);
        }
    }

  json_builder_end_array (builder);
  request = valent_packet_end (&builder);

  if (n_requested > 0)
    valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), request);
}

static void
valent_contact_store_add_contacts_cb (ValentContactStore *store,
                                      GAsyncResult       *result,
                                      gpointer            user_data)
{
  g_autoptr (GError) error = NULL;

  if (!valent_contact_store_add_contacts_finish (store, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
valent_contact_plugin_handle_response_vcards (ValentContactsPlugin *self,
                                              JsonNode             *packet)
{
  g_autoslist (EContact) contacts = NULL;
  JsonObject *body;
  JsonObjectIter iter;
  const char *uid;
  JsonNode *node;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  body = valent_packet_get_body (packet);
  json_object_iter_init (&iter, body);

  while (json_object_iter_next (&iter, &uid, &node))
    {
      EContact *contact;
      const char *vcard;

      /* NOTE: This has the side-effect of ignoring `uids` array, which is fine
       *       because the contact members are the ultimate source of truth. */
      if G_UNLIKELY (json_node_get_value_type (node) != G_TYPE_STRING)
        continue;

      vcard = json_node_get_string (node);
      contact = e_contact_new_from_vcard_with_uid (vcard, uid);

      contacts = g_slist_append (contacts, contact);
    }

  if (contacts != NULL)
    {
      valent_contact_store_add_contacts (self->remote_store,
                                         contacts,
                                         self->cancellable,
                                         (GAsyncReadyCallback)valent_contact_store_add_contacts_cb,
                                         NULL);
    }
}

static void
valent_contacts_plugin_request_all_uids_timestamps (ValentContactsPlugin *self)
{
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  packet = valent_packet_new ("kdeconnect.contacts.request_all_uids_timestamps");
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
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

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  valent_contacts_plugin_request_all_uids_timestamps (self);
}

static const GActionEntry actions[] = {
    {"fetch", contacts_fetch_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_contacts_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (plugin);
  ValentContacts *contacts = valent_contacts_get_default ();
  ValentContactStore *store = NULL;
  g_autofree char *local_uid = NULL;
  ValentDevice *device;
  GSettings *settings;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);

  /* Prepare Addressbooks */
  self->cancellable = g_cancellable_new ();
  device = valent_device_plugin_get_device (plugin);
  settings = valent_device_plugin_get_settings (VALENT_DEVICE_PLUGIN (self));

  store = valent_contacts_ensure_store (valent_contacts_get_default (),
                                        valent_device_get_id (device),
                                        valent_device_get_name (device));
  g_set_object (&self->remote_store, store);

  /* Local address book, shared with remote device */
  local_uid = g_settings_get_string (settings, "local-uid");

  if (*local_uid != '\0')
    {
      unsigned int n_stores = g_list_model_get_n_items (G_LIST_MODEL (contacts));

      for (unsigned int i = 0; i < n_stores; i++)
        {
          g_autoptr (ValentContactStore) local = NULL;

          local = g_list_model_get_item (G_LIST_MODEL (contacts), i);

          if (g_strcmp0 (valent_contact_store_get_uid (local), local_uid) != 0)
            continue;

          g_set_object (&self->local_store, local);
          break;
        }
    }
}

static void
valent_contacts_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (plugin);

  /* Cancel any pending operations and drop the address books */
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->remote_store);
  g_clear_object (&self->local_store);
}

static void
valent_contacts_plugin_update_state (ValentDevicePlugin *plugin,
                                     ValentDeviceState   state)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin, available);

  if (available)
    valent_contacts_plugin_request_all_uids_timestamps (self);
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

  /* A response to a request for a listing of contacts */
  if (g_str_equal (type, "kdeconnect.contacts.response_uids_timestamps"))
    valent_contact_plugin_handle_response_uids_timestamps (self, packet);

  /* A response to a request for contacts */
  else if (g_str_equal (type, "kdeconnect.contacts.response_vcards"))
    valent_contact_plugin_handle_response_vcards (self, packet);

  /* A request for a listing of contacts */
  else if (g_str_equal (type, "kdeconnect.contacts.request_all_uids_timestamps"))
    valent_contact_plugin_handle_request_all_uids_timestamps (self, packet);

  /* A request for contacts */
  else if (g_str_equal (type, "kdeconnect.contacts.request_vcards_by_uid"))
    valent_contact_plugin_handle_request_vcards_by_uid (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_contacts_plugin_class_init (ValentContactsPluginClass *klass)
{
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  plugin_class->enable = valent_contacts_plugin_enable;
  plugin_class->disable = valent_contacts_plugin_disable;
  plugin_class->handle_packet = valent_contacts_plugin_handle_packet;
  plugin_class->update_state = valent_contacts_plugin_update_state;
}

static void
valent_contacts_plugin_init (ValentContactsPlugin *self)
{
}

