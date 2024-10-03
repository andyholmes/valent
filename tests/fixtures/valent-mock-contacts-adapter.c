// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-contacts-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>

#include "valent-mock-contacts-adapter.h"


/**
 * ValentMockContactsAdapter:
 *
 * `ValentMockContactsAdapter` is a mock contact store adapter for testing
 * purposes. It loads with a single contact store, with a single contact.
 *
 * The store UID and name are `mock-store' and `Mock Store`, respectively. The
 * contact UID and name are `mock-contact` and `Mock Contact`, respectively,
 * with a telephone number of `123-456-7890`.
 *
 * Simply call valent_contacts_adapter_store_added() to add more
 * stores and valent_contacts_adapter_store_removed() to remove them.
 */

struct _ValentMockContactsAdapter
{
  ValentContactsAdapter  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockContactsAdapter, valent_mock_contacts_adapter, VALENT_TYPE_CONTACTS_ADAPTER)

static void
deserialize_cb (TrackerSparqlConnection *connection,
                GAsyncResult            *result,
                gpointer                 user_data)
{
  GError *error = NULL;

  tracker_sparql_connection_deserialize_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
update_cb (TrackerSparqlConnection *connection,
           GAsyncResult            *result,
           gpointer                 user_data)
{
  GError *error = NULL;

  tracker_sparql_connection_update_finish (connection, result, &error);
  g_assert_no_error (error);
}

static void
update_resource_cb (TrackerSparqlConnection *connection,
                    GAsyncResult            *result,
                    gpointer                 user_data)
{
  GError *error = NULL;

  tracker_sparql_connection_update_resource_finish (connection, result, &error);
  g_assert_no_error (error);
}

static inline TrackerResource *
_tracker_resource_from_vcard (const char *vcard)
{
  g_autoptr (TrackerResource) resource = NULL;
  g_autoptr (TrackerResource) contact_resource = NULL;
  TrackerResource *medium_resource = NULL;
  g_autoptr (EContact) contact = NULL;
  g_autofree char *contact_iri = NULL;
  g_autofree char *medium_iri = NULL;
  const char *uid = NULL;
  const char *medium = NULL;

  g_assert (vcard != NULL && *vcard != '\0');

  contact = e_contact_new_from_vcard (vcard);
  uid = e_contact_get_const (contact, E_CONTACT_UID);
  medium = e_contact_get_const (contact, E_CONTACT_PHONE_OTHER);

  contact_iri = g_strdup_printf ("urn:valent:contacts:mock:default:%s", uid);
  contact_resource = tracker_resource_new (contact_iri);
  tracker_resource_set_uri (contact_resource, "rdf:type", "nco:PersonContact");
  tracker_resource_set_string (contact_resource, "nie:plainTextContent", vcard);
  tracker_resource_set_string (contact_resource, "nco:contactUID", uid);
  tracker_resource_set_string (contact_resource, "nco:fullname",
                               e_contact_get_const (contact, E_CONTACT_FULL_NAME));

  medium_iri = tracker_sparql_escape_uri_printf ("tel:%s", medium);
  medium_resource = tracker_resource_new (medium_iri);
  tracker_resource_set_uri (medium_resource, "rdf:type", "nco:PhoneNumber");
  tracker_resource_set_string (medium_resource, "nco:phoneNumber", medium);
  tracker_resource_add_take_relation (contact_resource,
                                      "nco:hasPhoneNumber",
                                      g_steal_pointer (&medium_resource));

  resource = tracker_resource_new ("urn:valent:contacts:mock:default");
  tracker_resource_set_uri (resource, "rdf:type", "nco:ContactList");
  tracker_resource_add_take_relation (resource,
                                      "nco:containsContact",
                                      g_steal_pointer (&contact_resource));

  return g_steal_pointer (&resource);
}

static void
action_callback (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  ValentMockContactsAdapter *self = VALENT_MOCK_CONTACTS_ADAPTER (user_data);
  const char *name = g_action_get_name (G_ACTION (action));
  const char *text = g_variant_get_string (parameter, NULL);
  g_autoptr (TrackerSparqlConnection) connection = NULL;

  g_object_get (self, "connection", &connection, NULL);
  if (g_str_equal (name, "add-contact"))
    {
      g_autoptr (TrackerResource) resource = NULL;

      resource = _tracker_resource_from_vcard (text);
      tracker_sparql_connection_update_resource_async (connection,
                                                       VALENT_CONTACTS_GRAPH,
                                                       resource,
                                                       NULL,
                                                       (GAsyncReadyCallback)update_resource_cb,
                                                       NULL);
    }
  else if (g_str_equal (name, "remove-contact"))
    {
      g_autofree char *sparql = NULL;

      sparql = g_strdup_printf ("DELETE DATA {"
                                "  GRAPH <valent:contacts> {"
                                "    <urn:valent:contacts:mock:default:%s> a nco:Contact ;"
                                "  }"
                                "}",
                                text);
      tracker_sparql_connection_update_async (connection,
                                              sparql,
                                              NULL,
                                              (GAsyncReadyCallback)update_cb,
                                              NULL);
    }
  else if (g_str_equal (name, "add-list"))
    {
      g_autoptr (TrackerResource) resource = NULL;
      g_autofree char *iri = NULL;

      iri = g_strdup_printf ("urn:valent:contacts:mock:%s", text);
      resource = tracker_resource_new (iri);
      tracker_resource_set_uri (resource, "rdf:type", "nco:ContactList");
      tracker_sparql_connection_update_resource_async (connection,
                                                       VALENT_CONTACTS_GRAPH,
                                                       resource,
                                                       NULL,
                                                       (GAsyncReadyCallback)update_resource_cb,
                                                       NULL);
    }
  else if (g_str_equal (name, "remove-list"))
    {
      g_autofree char *sparql = NULL;

      sparql = g_strdup_printf ("DELETE DATA {"
                                "  GRAPH <valent:contacts> {"
                                "    <urn:valent:contacts:mock:%s> a nco:ContactList ;"
                                "  }"
                                "}",
                                text);
      tracker_sparql_connection_update_async (connection,
                                              sparql,
                                              NULL,
                                              (GAsyncReadyCallback)update_cb,
                                              NULL);
    }
}

static const GActionEntry actions[] = {
    {"add-contact",    action_callback, "s", NULL, NULL},
    {"remove-contact", action_callback, "s", NULL, NULL},
    {"add-list",       action_callback, "s", NULL, NULL},
    {"remove-list",    action_callback, "s", NULL, NULL},
};

/*
 * GObject
 */

static void
valent_mock_contacts_adapter_constructed (GObject *object)
{
  ValentMockContactsAdapter *self = VALENT_MOCK_CONTACTS_ADAPTER (object);
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (GInputStream) graph = NULL;
  g_autoptr (GListModel) list = NULL;

  G_OBJECT_CLASS (valent_mock_contacts_adapter_parent_class)->constructed (object);

  g_object_get (self, "connection", &connection, NULL);
  graph = g_resources_open_stream ("/plugins/mock/graph-contacts.turtle",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   NULL);
  tracker_sparql_connection_deserialize_async (connection,
                                               TRACKER_DESERIALIZE_FLAGS_NONE,
                                               TRACKER_RDF_FORMAT_TURTLE,
                                               VALENT_CONTACTS_GRAPH,
                                               graph,
                                               NULL,
                                               (GAsyncReadyCallback)deserialize_cb,
                                               NULL);

  while (g_list_model_get_n_items (G_LIST_MODEL (self)) == 0)
    g_main_context_iteration (NULL, FALSE);

  list = g_list_model_get_item (G_LIST_MODEL (self), 0);
  while (g_list_model_get_n_items (G_LIST_MODEL (list)) < 3)
    g_main_context_iteration (NULL, FALSE);
}

static void
valent_mock_contacts_adapter_class_init (ValentMockContactsAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = valent_mock_contacts_adapter_constructed;
}

static void
valent_mock_contacts_adapter_init (ValentMockContactsAdapter *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}

