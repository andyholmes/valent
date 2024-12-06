// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-device"

#include "config.h"

#include <inttypes.h>

#include <gio/gio.h>
#include <libebook-contacts/libebook-contacts.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>

#include "valent-contacts-device.h"

struct _ValentContactsDevice
{
  ValentContactsAdapter    parent_instance;

  char                    *default_iri;
  TrackerSparqlStatement  *get_timestamp_stmt;
  GCancellable            *cancellable;
};

G_DEFINE_FINAL_TYPE (ValentContactsDevice, valent_contacts_device, VALENT_TYPE_CONTACTS_ADAPTER)

static void
execute_add_contacts_cb (TrackerSparqlConnection *connection,
                         GAsyncResult            *result,
                         gpointer                 user_data)
{
  g_autoptr (GError) error = NULL;

  if (!tracker_sparql_connection_update_resource_finish (connection, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_debug ("%s(): %s", G_STRFUNC, error->message);
    }
}

/*
 * ValentContactsAdapter
 */
static void
valent_contacts_device_send_packet_cb (ValentDevice *device,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!valent_device_send_packet_finish (device, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED))
        g_critical ("%s(): %s", G_STRFUNC, error->message);
      else if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_debug ("%s(): %s", G_STRFUNC, error->message);
    }
}

static void
valent_contacts_device_handle_response_uids_timestamps (ValentContactsDevice *self,
                                                        JsonNode             *packet)
{
  g_autoptr (JsonNode) request = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  JsonObjectIter iter;
  const char *uid;
  JsonNode *node;
  unsigned int n_requested = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS_DEVICE (self));

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

      // TODO
      if (timestamp != 0)
        {
          json_builder_add_string_value (builder, uid);
          n_requested++;
        }
    }

  json_builder_end_array (builder);
  request = valent_packet_end (&builder);

  if (n_requested > 0)
    {
      ValentDevice *device = NULL;

      device = valent_resource_get_source (VALENT_RESOURCE (self));
      valent_device_send_packet (device,
                                 request,
                                 NULL,
                                 (GAsyncReadyCallback) valent_contacts_device_send_packet_cb,
                                 NULL);
    }

  VALENT_EXIT;
}

static void
valent_contacts_device_handle_response_vcards (ValentContactsDevice *self,
                                               JsonNode             *packet)
{
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (TrackerResource) list_resource = NULL;
  ValentDevice *device;
  const char *list_name;
  JsonObjectIter iter;
  const char *uid;
  JsonNode *node;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS_DEVICE (self));
  g_assert (VALENT_IS_PACKET (packet));

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  list_name = valent_device_get_name (device);

  list_resource = tracker_resource_new (self->default_iri);
  tracker_resource_set_uri (list_resource, "rdf:type", "nco:ContactList");
  tracker_resource_set_string (list_resource, "nie:title", list_name);

  json_object_iter_init (&iter, valent_packet_get_body (packet));
  while (json_object_iter_next (&iter, &uid, &node))
    {
      TrackerResource *item_resource = NULL;
      g_autoptr (EContact) contact = NULL;
      const char *vcard;

      /* NOTE: This has the side-effect of ignoring `uids` array
       */
      if G_UNLIKELY (json_node_get_value_type (node) != G_TYPE_STRING)
        continue;

      vcard = json_node_get_string (node);
      contact = e_contact_new_from_vcard_with_uid (vcard, uid);
      item_resource = valent_contact_resource_from_econtact (contact);
      if (item_resource != NULL)
        {
          g_autofree char *item_urn = NULL;

          item_urn = tracker_sparql_escape_uri_printf ("%s:%s",
                                                       self->default_iri,
                                                       uid);
          tracker_resource_set_identifier (item_resource, item_urn);
          tracker_resource_add_take_relation (list_resource,
                                              "nco:containsContact",
                                              g_steal_pointer (&item_resource));
        }
    }

  g_object_get (self, "connection", &connection, NULL);
  tracker_sparql_connection_update_resource_async (connection,
                                                   VALENT_CONTACTS_GRAPH,
                                                   list_resource,
                                                   self->cancellable,
                                                   (GAsyncReadyCallback) execute_add_contacts_cb,
                                                   NULL);

  VALENT_EXIT;
}

static void
valent_contacts_device_request_all_uids_timestamps (ValentContactsDevice *self)
{
  ValentDevice *device = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_CONTACTS_DEVICE (self));

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  packet = valent_packet_new ("kdeconnect.contacts.request_all_uids_timestamps");
  valent_device_send_packet (device,
                             packet,
                             NULL,
                             (GAsyncReadyCallback) valent_contacts_device_send_packet_cb,
                             NULL);
}

static void
on_device_state_changed (ValentDevice         *device,
                         GParamSpec           *pspec,
                         ValentContactsDevice *self)
{
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  gboolean available;

  state = valent_device_get_state (device);
  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available)
    valent_contacts_device_request_all_uids_timestamps (self);
}

/*
 * GObject
 */
static void
valent_contacts_device_constructed (GObject *object)
{
  ValentContactsDevice *self = VALENT_CONTACTS_DEVICE (object);
  ValentDevice *device = NULL;
  g_autofree char *iri = NULL;

  G_OBJECT_CLASS (valent_contacts_device_parent_class)->constructed (object);

  device = valent_resource_get_source (VALENT_RESOURCE (self));
  g_signal_connect_object (device,
                           "notify::state",
                           G_CALLBACK (on_device_state_changed),
                           self,
                           G_CONNECT_DEFAULT);

  iri = valent_object_dup_iri (VALENT_OBJECT (self));
  self->default_iri = tracker_sparql_escape_uri_printf ("%s:default", iri);
}

static void
valent_contacts_device_finalize (GObject *object)
{
  ValentContactsDevice *self = VALENT_CONTACTS_DEVICE (object);

  g_clear_pointer (&self->default_iri, g_free);
  g_clear_object (&self->get_timestamp_stmt);

  G_OBJECT_CLASS (valent_contacts_device_parent_class)->finalize (object);
}

static void
valent_contacts_device_class_init (ValentContactsDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_contacts_device_constructed;
  object_class->finalize = valent_contacts_device_finalize;
}

static void
valent_contacts_device_init (ValentContactsDevice *self)
{
}

/*< private >
 * valent_contacts_device_new:
 * @device: a `ValentDevice`
 *
 * Create a new `ValentContactsDevice`.
 *
 * Returns: (transfer full): a new adapter
 */
ValentContactsAdapter *
valent_contacts_device_new (ValentDevice *device)
{
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *iri = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  context = valent_context_new (valent_device_get_context (device),
                                "plugin",
                                "contacts");
  iri = tracker_sparql_escape_uri_printf ("urn:valent:contacts:%s",
                                          valent_device_get_id (device));
  return g_object_new (VALENT_TYPE_CONTACTS_DEVICE,
                       "iri",     iri,
                       "context", context,
                       "source",  device,
                       "title",   valent_device_get_name (device),
                       NULL);
}

/*< private >
 * valent_contacts_device_handle_packet:
 * @adapter: a `ValentContactsAdapter`
 * @type: a KDE Connect packet type
 * @packet: a KDE Connect packet
 *
 * Handle an incoming `kdeconnect.contacts.*` packet.
 */
void
valent_contacts_device_handle_packet (ValentContactsAdapter *adapter,
                                      const char            *type,
                                      JsonNode              *packet)
{
  ValentContactsDevice *self = VALENT_CONTACTS_DEVICE (adapter);

  g_assert (VALENT_IS_CONTACTS_DEVICE (adapter));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  /* A response to a request for a listing of contacts
   */
  if (g_str_equal (type, "kdeconnect.contacts.response_uids_timestamps"))
    valent_contacts_device_handle_response_uids_timestamps (self, packet);

  /* A response to a request for vCards
   */
  else if (g_str_equal (type, "kdeconnect.contacts.response_vcards"))
    valent_contacts_device_handle_response_vcards (self, packet);

  else
    g_assert_not_reached ();
}

