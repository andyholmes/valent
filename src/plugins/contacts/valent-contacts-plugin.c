// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-plugin"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>

#include "valent-contacts-plugin.h"


struct _ValentContactsPlugin
{
  PeasExtensionBase   parent_instance;
  ValentDevice       *device;
  GSettings          *settings;

  ValentContactStore *remote_store;
  ValentContactStore *local_store;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentContactsPlugin, valent_contacts_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


static char *
rev_from_timestamp (gint64 timestamp)
{
  g_autoptr (GDateTime) datetime = NULL;

  datetime = g_date_time_new_from_unix_local (timestamp);

  return g_date_time_format (datetime, "%Y%m%dT%H%M%SZ");
}

static EContactDate *
date_from_timestamp (gint64 timestamp)
{
  g_autoptr (GDateTime) datetime = NULL;
  g_autofree char *ymd = NULL;

  datetime = g_date_time_new_from_unix_local (timestamp);
  ymd = g_date_time_format (datetime, "%F");

  return e_contact_date_from_string (ymd);
}


/*
 * Packet Handlers
 */
static void
handle_response_uids_timestamps (ValentContactsPlugin *self,
                                 JsonNode             *packet)
{
  g_autoptr (JsonNode) request = NULL;
  JsonBuilder *builder;
  JsonObjectIter iter;
  const char *uid;
  JsonNode *node;
  unsigned int n_requested = 0;

  /* Start the packet */
  builder = valent_packet_start ("kdeconnect.contacts.request_vcards_by_uid");
  json_builder_set_member_name (builder, "uids");
  json_builder_begin_array (builder);

  json_object_iter_init (&iter, valent_packet_get_body (packet));

  while (json_object_iter_next (&iter, &uid, &node))
    {
      gint64 timestamp = 0;

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
  request = valent_packet_finish (builder);

  if (n_requested > 0)
    valent_device_queue_packet (self->device, request);
}

static void
add_cb (ValentContactStore *store,
        GAsyncResult       *result,
        gpointer            user_data)
{
  g_autoptr (GError) error = NULL;

  if (!valent_contact_store_add_finish (store, result, &error))
    g_warning ("%s: %s", G_STRFUNC, error->message);
}

static void
handle_response_vcards (ValentContactsPlugin *self,
                        JsonNode             *packet)
{
  g_autoslist (EContact) contacts = NULL;
  JsonObject *body;
  JsonObjectIter iter;
  const char *uid;
  JsonNode *node;

  body = valent_packet_get_body (packet);
  json_object_iter_init (&iter, body);

  while (json_object_iter_next (&iter, &uid, &node))
    {
      EContact *contact;
      const char *vcard;

      /* NOTE: we're completely ignoring the `uids` field, because it's faster
       *       and safer that comparing the strv to the object members */
      if G_UNLIKELY (g_str_equal (uid, "uids"))
        continue;

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
                                         NULL,
                                         (GAsyncReadyCallback)add_cb,
                                         NULL);
    }
}

static void
query_cb (ValentContactStore   *store,
          GAsyncResult         *result,
          ValentContactsPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) response = NULL;
  g_autoslist (GObject) contacts = NULL;
  g_autoptr (GError) error = NULL;

  /* Warn on error and send an empty list */
  contacts = valent_contact_store_query_finish (store, result, &error);

  if (error != NULL)
    g_warning ("Reading contacts: %s", error->message);

  /* Build response */
  builder = valent_packet_start ("kdeconnect.contacts.response_uids_timestamps");

  for (const GSList *iter = contacts; iter; iter = iter->next)
    {
      const char *uid;
      gint64 timestamp = 0;

      uid = e_contact_get_const (iter->data, E_CONTACT_UID);
      json_builder_set_member_name (builder, uid);

      // TODO: We probably need to convert between the custom field
      // `X-KDECONNECT-TIMESTAMP` and `E_CONTACT_REV` to set a proper timestamp
      timestamp = 0;
      json_builder_add_int_value (builder, timestamp);
    }

  response = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, response);
}

static void
handle_request_all_uids_timestamps (ValentContactsPlugin *self,
                                    JsonNode             *packet)
{
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  /* Bail if exporting is disabled */
  if (self->local_store == NULL ||
      !g_settings_get_boolean (self->settings, "local-sync"))
    return;

  query = e_book_query_vcard_field_exists (EVC_UID);
  sexp = e_book_query_to_string (query);

  valent_contact_store_query (self->local_store,
                              sexp,
                              NULL,
                              (GAsyncReadyCallback)query_cb,
                              self);
}

static void
vcards_by_uid_cb (ValentContactStore   *store,
                  GAsyncResult         *result,
                  ValentContactsPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) response = NULL;
  g_autoslist (GObject) contacts = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CONTACT_STORE (store));
  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  contacts = valent_contact_store_query_finish (store, result, &error);

  if (error != NULL)
    {
      g_warning ("Querying contacts: %s", error->message);
      return;
    }

  builder = valent_packet_start ("kdeconnect.contacts.response_vcards");

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
  response = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, response);
}

/**
 * handle_request_vcards_by_uid:
 * @self: a #ValentContactsPlugin
 * @packet: a #JsonNode
 *
 * Handle a request for local contacts from the remote device.
 */
static void
handle_request_vcards_by_uid (ValentContactsPlugin *self,
                              JsonNode             *packet)
{
  g_autofree EBookQuery **queries = NULL;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;
  JsonObject *body;
  JsonNode *node;
  JsonArray *uids;
  unsigned int n_uids, n_queries;

  /* Bail if exporting is disabled */
  if (self->local_store == NULL || !g_settings_get_boolean (self->settings, "local-sync"))
    return;

  /* Check packet */
  body = valent_packet_get_body (packet);

  if ((node = json_object_get_member (body, "uids")) == NULL ||
      json_node_get_node_type (node) != JSON_NODE_ARRAY)
    {
      g_debug ("%s: packet missing \"uids\" field", G_STRFUNC);
      return;
    }

  /* Build a list of queries */
  uids = json_node_get_array (node);
  n_uids = json_array_get_length (uids);
  queries = g_new (EBookQuery *, n_uids);
  n_queries = 0;

  for (unsigned int i = 0; i < n_uids; i++)
    {
      JsonNode *element = json_array_get_element (uids, i);
      const char *uid = NULL;

      if G_LIKELY (json_node_get_value_type (element) == G_TYPE_STRING)
        uid = json_node_get_string (element);

      if (uid == NULL || g_str_equal (uid, ""))
        {
          g_debug ("%s: \"uids\" field contains invalid entry", G_STRFUNC);
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
                              NULL,
                              (GAsyncReadyCallback)vcards_by_uid_cb,
                              self);
}

/**
 * Packet Providers
 */
static void
valent_contacts_plugin_request_all_uids_timestamps (ValentContactsPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.contacts.request_all_uids_timestamps");
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_contacts_plugin_request_vcards_by_uid (ValentContactsPlugin *self,
                                              const GStrv           uids)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_CONTACTS_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.contacts.request_vcards_by_uid");

  json_builder_set_member_name (builder, "uids");
  json_builder_begin_array (builder);

  for (unsigned int i = 0; uids[i]; i++)
    json_builder_add_string_value (builder, uids[i]);

  json_builder_end_array (builder);

  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
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
    {"contacts-fetch", contacts_fetch_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_contacts_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (plugin);
  ValentContactStore *store = NULL;
  g_autofree char *local_uid = NULL;
  const char *device_id;

  g_assert (VALENT_IS_CONTACTS_PLUGIN (self));

  /* Setup GSettings */
  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "contacts");

  /* Remote Addressbook */
  if (self->remote_store == NULL)
    {
      store = valent_contacts_ensure_store (valent_contacts_get_default (),
                                            valent_device_get_id (self->device),
                                            valent_device_get_name (self->device));
      self->remote_store = g_object_ref (store);
    }

  /* Local Addressbook */
  if (self->local_store == NULL)
    {
      local_uid = g_settings_get_string (self->settings, "local-uid");
      self->local_store = valent_contacts_get_store (valent_contacts_get_default (), local_uid);
    }

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));
}

static void
valent_contacts_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (plugin);

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  /* Drop Addressbooks */
  g_clear_object (&self->remote_store);
  //g_clear_object (&self->local_store);

  /* Dispose GSettings */
  g_clear_object (&self->settings);
}

static void
valent_contacts_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* GActions */
  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);

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
  if (g_strcmp0 (type, "kdeconnect.contacts.response_uids_timestamps") == 0)
    handle_response_uids_timestamps (self, packet);

  /* A response to a request for contacts */
  else if (g_strcmp0 (type, "kdeconnect.contacts.response_vcards") == 0)
    handle_response_vcards (self, packet);

  /* A request for a listing of contacts */
  else if (g_strcmp0 (type, "kdeconnect.contacts.request_all_uids_timestamps") == 0)
    handle_request_all_uids_timestamps (self, packet);

  /* A request for contacts */
  else if (g_strcmp0 (type, "kdeconnect.contacts.request_vcards_by_uid") == 0)
    handle_request_vcards_by_uid (self, packet);

  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_contacts_plugin_enable;
  iface->disable = valent_contacts_plugin_disable;
  iface->handle_packet = valent_contacts_plugin_handle_packet;
  iface->update_state = valent_contacts_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_contacts_plugin_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contacts_plugin_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ValentContactsPlugin *self = VALENT_CONTACTS_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contacts_plugin_class_init (ValentContactsPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_contacts_plugin_get_property;
  object_class->set_property = valent_contacts_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_contacts_plugin_init (ValentContactsPlugin *self)
{
}

