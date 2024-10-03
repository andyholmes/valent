// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-device"

#include "config.h"

#include <inttypes.h>

#include <gio/gio.h>
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

static TrackerResource *
valent_contacts_device_vcard_to_resource (const char *base_iri,
                                          const char *vcard,
                                          const char *uid)
{
  g_autoptr (TrackerResource) resource = NULL;
  g_autoptr (EContact) contact = NULL;
  g_autolist (EVCardAttribute) phone_numbers = NULL;
  g_autolist (EVCardAttribute) email_addresses = NULL;
  // g_autolist (EVCardAttribute) postal_addresses = NULL;
  g_autoptr (EContactDate) birthdate = NULL;
  const char *url = NULL;
  g_autofree char *iri = NULL;
  static struct
  {
    EContactField field;
    const char *property;
  } contact_fields[] = {
#if 0
    {
      .field = E_CONTACT_BIRTH_DATE,
      .property = "nco:birthDate",
    },
#endif
    {
      .field = E_CONTACT_UID,
      .property = "nco:contactUID",
    },
    {
      .field = E_CONTACT_FULL_NAME,
      .property = "nco:fullname",
    },
    {
      .field = E_CONTACT_NICKNAME,
      .property = "nco:nickname",
    },
    {
      .field = E_CONTACT_NOTE,
      .property = "nco:note",
    },
#if 0
    {
      .field = E_CONTACT_PHOTO,
      .property = "nco:photo",
    },
    {
      .field = E_CONTACT_HOMEPAGE_URL,
      .property = "nco:url",
    },
#endif
  };

  g_assert (base_iri != NULL && *base_iri != '\0');
  g_assert (vcard != NULL);
  g_assert (uid == NULL || *uid != '\0');

  /* NOTE: nco:PersonContact is used unconditionally, because it's the only
   *       class which receives change notification.
   */
  iri = g_strdup_printf ("%s:%s", base_iri, uid);
  resource = tracker_resource_new (iri);
  tracker_resource_set_uri (resource, "rdf:type", "nco:PersonContact");
  tracker_resource_set_string (resource, "nie:plainTextContent", vcard);

  contact = e_contact_new_from_vcard_with_uid (vcard, uid);
  for (size_t i = 0; i < G_N_ELEMENTS (contact_fields); i++)
    {
      const char *value = NULL;

      value = e_contact_get_const (contact, contact_fields[i].field);
      if (value != NULL && *value != '\0')
        tracker_resource_set_string (resource, contact_fields[i].property, value);
    }

  birthdate = e_contact_get (contact, E_CONTACT_BIRTH_DATE);
  if (birthdate != NULL)
    {
      g_autoptr (GDateTime) date = NULL;

      date = g_date_time_new_local (birthdate->year,
                                    birthdate->month,
                                    birthdate->day,
                                    0, 0, 0.0);
      tracker_resource_set_datetime (resource, "nco:birthDate", date);
    }

  url = e_contact_get_const (contact, E_CONTACT_HOMEPAGE_URL);
  if (url != NULL && g_uri_is_valid (url, G_URI_FLAGS_PARSE_RELAXED, NULL))
    tracker_resource_set_uri (resource, "nco:url", url);

  phone_numbers = e_contact_get_attributes (contact, E_CONTACT_TEL);
  for (const GList *iter = phone_numbers; iter; iter = iter->next)
    {
      EVCardAttribute *attr = iter->data;
      g_autofree char *medium = NULL;
      g_autoptr (EPhoneNumber) number = NULL;
      g_autofree char *medium_iri = NULL;
      TrackerResource *medium_resource = NULL;
      const char *medium_type = NULL;

      if (e_vcard_attribute_has_type (attr, "CAR"))
        medium_type = "nco:CarPhoneNumber";
      else if (e_vcard_attribute_has_type (attr, "CELL"))
        medium_type = "nco:MessagingNumber";
      else if (e_vcard_attribute_has_type (attr, "FAX"))
        medium_type = "nco:FaxNumber";
      else if (e_vcard_attribute_has_type (attr, "ISDN"))
        medium_type = "nco:IsdnNumber";
      else if (e_vcard_attribute_has_type (attr, "PAGER"))
        medium_type = "nco:PagerNumber";
      else if (e_vcard_attribute_has_type (attr, "VOICE"))
        medium_type = "nco:VoicePhoneNumber";
      else
        medium_type = "nco:PhoneNumber";

      medium = e_vcard_attribute_get_value (attr);
      number = e_phone_number_from_string (medium, NULL, NULL);
      if (number != NULL)
        medium_iri = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_RFC3966);
      else
        medium_iri = g_strdup_printf ("tel:%s", medium);

      medium_resource = tracker_resource_new (medium_iri);
      tracker_resource_set_uri (medium_resource, "rdf:type", medium_type);
      tracker_resource_set_string (medium_resource, "nco:phoneNumber", medium);
      tracker_resource_add_take_relation (resource,
                                          "nco:hasPhoneNumber",
                                          g_steal_pointer (&medium_resource));
    }

  email_addresses = e_contact_get_attributes (contact, E_CONTACT_EMAIL);
  for (const GList *iter = email_addresses; iter; iter = iter->next)
    {
      EVCardAttribute *attr = iter->data;
      g_autofree char *medium = NULL;
      g_autofree char *medium_iri = NULL;
      TrackerResource *medium_resource = NULL;

      medium = e_vcard_attribute_get_value (attr);
      medium_iri = g_strdup_printf ("mailto:%s", medium);
      medium_resource = tracker_resource_new (medium_iri);
      tracker_resource_set_uri (medium_resource, "rdf:type", "nco:EmailAddress");
      tracker_resource_set_string (medium_resource, "nco:emailAddress", medium);
      tracker_resource_add_take_relation (resource,
                                          "nco:hasEmailAddress",
                                          g_steal_pointer (&medium_resource));
    }

#if 0
  postal_addresses = e_contact_get_attributes (contact, E_CONTACT_ADDRESS);
  for (const GList *iter = postal_addresses; iter; iter = iter->next)
    {
      EVCardAttribute *attr = iter->data;
      GList *values = e_vcard_attribute_get_values_decoded (attr);
      g_autofree char *medium = NULL;
      g_autofree char *medium_iri = NULL;
      TrackerResource *medium_resource = NULL;
      const char *medium_type = NULL;
      uint8_t v = 0;

      if (e_vcard_attribute_has_type (attr, "DOM"))
        medium_type = "nco:DomesticDeliveryAddress";
      else if (e_vcard_attribute_has_type (attr, "INTL"))
        medium_type = "nco:InternationalDeliveryAddress";
      else if (e_vcard_attribute_has_type (attr, "PARCEL"))
        medium_type = "nco:ParcelDeliveryAddress";
      else
        medium_type = "nco:PostalAddress";

      medium = e_vcard_attribute_get_value (attr);
      medium_iri = g_strdup_printf ("mailto:%s", medium);
      medium_resource = tracker_resource_new (medium_iri);
      tracker_resource_set_uri (medium_resource, "rdf:type", medium_type);

      for (const GList *viter = values; viter != NULL; viter = viter->next)
        {
          const GString *decoded = values->data;
          static const char *adr_fields[] = {
            "nco:pobox",
            "nco:extendedAddress",
            "nco:streetAddress",
            "nco:locality",
            "nco:region",
            "nco:postalcode",
            "nco:country",
          };

          if (v < G_N_ELEMENTS (adr_fields) && decoded->len > 0)
            {
              tracker_resource_set_string (medium_resource,
                                           adr_fields[v],
                                           decoded->str);
            }

          v++;
        }

      tracker_resource_add_take_relation (resource,
                                          "nco:hasPostalAddress",
                                          g_steal_pointer (&medium_resource));
    }
#endif

  return g_steal_pointer (&resource);
}

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

      device = valent_extension_get_object (VALENT_EXTENSION (self));
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
  g_autoptr (TrackerResource) resource = NULL;
  JsonObjectIter iter;
  const char *uid;
  JsonNode *node;

  VALENT_ENTRY;

  g_assert (VALENT_IS_CONTACTS_DEVICE (self));
  g_assert (VALENT_IS_PACKET (packet));

  resource = tracker_resource_new (self->default_iri);
  tracker_resource_set_uri (resource, "rdf:type", "nco:ContactList");

  json_object_iter_init (&iter, valent_packet_get_body (packet));
  while (json_object_iter_next (&iter, &uid, &node))
    {
      TrackerResource *contact = NULL;
      const char *vcard;

      /* NOTE: This has the side-effect of ignoring `uids` array
       */
      if G_UNLIKELY (json_node_get_value_type (node) != G_TYPE_STRING)
        continue;

      vcard = json_node_get_string (node);
      contact = valent_contacts_device_vcard_to_resource (self->default_iri,
                                                          vcard, uid);
      if (contact != NULL)
        {
          tracker_resource_add_take_relation (resource,
                                              "nco:containsContact",
                                              g_steal_pointer (&contact));
        }
    }

  g_object_get (self, "connection", &connection, NULL);
  tracker_sparql_connection_update_resource_async (connection,
                                                   VALENT_CONTACTS_GRAPH,
                                                   resource,
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

  device = valent_extension_get_object (VALENT_EXTENSION (self));
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

  device = valent_extension_get_object (VALENT_EXTENSION (self));
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
                       "object",  device,
                       "context", context,
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

