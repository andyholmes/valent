// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact"

#include "config.h"

#include <gio/gio.h>
#include <libebook-contacts/libebook-contacts.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "valent-contact.h"


//

/**
 * valent_contact_resource_from_econtact:
 * @contact: an `EContact`
 *
 * Pack an [class@EBookContacts.Contact] into a [class@Tracker.Resource].
 *
 * Returns: (transfer full): a new SPARQL resource
 *
 * Since: 1.0
 */
TrackerResource *
valent_contact_resource_from_econtact (EContact *contact)
{
  g_autoptr (TrackerResource) resource = NULL;
  g_autolist (EVCardAttribute) phone_numbers = NULL;
  g_autolist (EVCardAttribute) email_addresses = NULL;
  g_autolist (EVCardAttribute) urls = NULL;
  // g_autolist (EVCardAttribute) postal_addresses = NULL;
  g_autoptr (EContactDate) birthdate = NULL;
  g_autofree char *vcard = NULL;
  static struct
  {
    EContactField field;
    const char *property;
  } contact_fields[] = {
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
#endif
  };

  g_return_val_if_fail (E_IS_CONTACT (contact), NULL);

  /* NOTE: nco:PersonContact is used unconditionally, because it's the only
   *       class which receives change notification.
   */
  resource = tracker_resource_new (NULL);
  tracker_resource_set_uri (resource, "rdf:type", "nco:PersonContact");

  vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_21);
  tracker_resource_set_string (resource, "nie:plainTextContent", vcard);

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

  phone_numbers = e_contact_get_attributes (contact, E_CONTACT_TEL);
  for (const GList *iter = phone_numbers; iter != NULL; iter = iter->next)
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
  for (const GList *iter = email_addresses; iter != NULL; iter = iter->next)
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

  urls = e_contact_get_attributes (contact, E_CONTACT_HOMEPAGE_URL);
  for (const GList *iter = urls; iter != NULL; iter = iter->next)
    {
      EVCardAttribute *attr = iter->data;
      g_autoptr (GString) url = NULL;

      url = e_vcard_attribute_get_value_decoded (attr);
      if (url != NULL && g_uri_is_valid (url->str, G_URI_FLAGS_PARSE_RELAXED, NULL))
        tracker_resource_add_uri (resource, "nco:url", url->str);
    }

  return g_steal_pointer (&resource);
}

