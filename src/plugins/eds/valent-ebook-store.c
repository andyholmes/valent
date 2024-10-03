// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ebook-store"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>

#include "valent-ebook-store.h"

#define WAIT_FOR_CONNECTED_TIMEOUT 30
#define ALL_CONTACTS_SEXP    "(exists 'tel')"


struct _ValentEBookStore
{
  ValentObject             parent_instance;

  TrackerSparqlConnection *connection;
  ESource                 *source;
  EBookClient             *client;
  EBookClientView         *view;
};

static void   g_async_initable_iface_init (GAsyncInitableIface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentEBookStore, valent_ebook_store, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, g_async_initable_iface_init))

typedef enum {
  PROP_CONNECTION = 1,
  PROP_SOURCE,
} ValentEBookStoreProperty;

static GParamSpec *properties[PROP_SOURCE + 1] = { NULL, };

static TrackerResource *
_e_contact_to_resource (EContact   *contact,
                        const char *urn)
{
  g_autoptr (TrackerResource) resource = NULL;
  g_autolist (EVCardAttribute) phone_numbers = NULL;
  g_autolist (EVCardAttribute) email_addresses = NULL;
  g_autoptr (EContactDate) birthdate = NULL;
  const char *url = NULL;
  g_autofree char *iri = NULL;
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
  };

  g_assert (E_IS_CONTACT (contact));
  g_assert (urn != NULL && *urn != '\0');

  /* NOTE: nco:PersonContact is used unconditionally, because it's the only
   *       class which receives change notification.
   */
  iri = tracker_sparql_escape_uri_printf ("%s:%s", urn,
                                          e_contact_get_const (contact, E_CONTACT_UID));
  resource = tracker_resource_new (iri);
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

static void
on_objects_added (EBookClientView  *view,
                  GSList           *contacts,
                  ValentEBookStore *self)
{
  g_autoptr (TrackerResource) book_resource = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autofree char *book_urn = NULL;
  const char *book_name = NULL;
  const char *book_uid = NULL;

  g_assert (E_IS_BOOK_CLIENT_VIEW (view));
  g_assert (VALENT_IS_EBOOK_STORE (self));

  book_name = e_source_get_display_name (self->source);
  book_uid = e_source_get_uid (self->source);
  book_urn = tracker_sparql_escape_uri_printf ("urn:valent:contacts:eds:%s",
                                               book_uid);
  book_resource = tracker_resource_new (book_urn);
  tracker_resource_set_uri (book_resource, "rdf:type", "nco:ContactList");
  if (book_name != NULL)
    tracker_resource_set_string (book_resource, "nie:title", book_name);

  for (const GSList *iter = contacts; iter; iter = iter->next)
    {
      g_autoptr (TrackerResource) resource = NULL;

      resource = _e_contact_to_resource ((EContact *)iter->data, book_urn);
      tracker_resource_add_take_relation (book_resource,
                                          "nco:containsContact",
                                          g_steal_pointer (&resource));
    }

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  tracker_sparql_connection_update_resource_async (self->connection,
                                                   VALENT_CONTACTS_GRAPH,
                                                   book_resource,
                                                   destroy,
                                                   (GAsyncReadyCallback) execute_add_contacts_cb,
                                                   NULL);
}

static void
execute_remove_contacts_cb (TrackerBatch *batch,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!tracker_batch_execute_finish (batch, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
on_objects_removed (EBookClientView  *view,
                    GSList           *uids,
                    ValentEBookStore *self)
{
  g_autoptr (TrackerBatch) batch = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  const char *book_uid = NULL;

  g_assert (E_IS_BOOK_CLIENT_VIEW (view));
  g_assert (VALENT_IS_EBOOK_STORE (self));

  book_uid = e_source_get_uid (self->source);
  batch = tracker_sparql_connection_create_batch (self->connection);
  for (const GSList *iter = uids; iter; iter = iter->next)
    {
      const char *contact_uid = (const char *)iter->data;
      g_autofree char *sparql = NULL;

      sparql = g_strdup_printf ("DELETE DATA {"
                                "  GRAPH <valent:contacts> {"
                                "    <urn:valent:contacts:eds:%s:%s> a nco:Contact ;"
                                "  }"
                                "}",
                                book_uid,
                                contact_uid);
      tracker_batch_add_sparql (batch, sparql);
    }

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  tracker_batch_execute_async (batch,
                               destroy,
                               (GAsyncReadyCallback) execute_remove_contacts_cb,
                               NULL);
}

static void
e_client_wait_for_connected_cb (EClient      *client,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!e_client_wait_for_connected_finish (client, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
e_book_client_get_view_cb (EBookClient      *client,
                           GAsyncResult     *result,
                           ValentEBookStore *self)
{
  g_autoptr (EBookClientView) view = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GError) error = NULL;

  if (!e_book_client_get_view_finish (client, result, &view, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  self->view = g_steal_pointer (&view);
  g_signal_connect_object (self->view,
                           "objects-added",
                           G_CALLBACK (on_objects_added),
                           self,
                           G_CONNECT_DEFAULT);
  g_signal_connect_object (self->view,
                           "objects-removed",
                           G_CALLBACK (on_objects_removed),
                           self,
                           G_CONNECT_DEFAULT);

  e_book_client_view_start (self->view, &error);
  if (error != NULL)
    g_warning ("%s(): %s", G_STRFUNC, error->message);

  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  e_client_wait_for_connected (E_CLIENT (self->client),
                               WAIT_FOR_CONNECTED_TIMEOUT,
                               destroy,
                               (GAsyncReadyCallback)e_client_wait_for_connected_cb,
                               self);
}

/*
 * GAsyncInitable
 */
static void
e_book_client_connect_cb (GObject      *object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (user_data);
  ValentEBookStore *self = g_task_get_source_object (user_data);
  g_autoptr (EClient) client = NULL;
  g_autoptr (GCancellable) destroy = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_EBOOK_STORE (self));

  client = e_book_client_connect_finish (result, &error);
  if (client == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  self->client = g_object_ref ((EBookClient *)client);
  g_task_return_boolean (task, TRUE);

  /* Initialization is finished; connect to the backend in the background
   */
  destroy = valent_object_ref_cancellable (VALENT_OBJECT (self));
  e_book_client_get_view (self->client,
                          "",
                          destroy,
                          (GAsyncReadyCallback)e_book_client_get_view_cb,
                          self);
}

static void
valent_ebook_store_init_async (GAsyncInitable      *initable,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (initable);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  g_assert (VALENT_IS_EBOOK_STORE (initable));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  destroy = valent_object_chain_cancellable (VALENT_OBJECT (initable),
                                             cancellable);

  task = g_task_new (initable, destroy, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_set_source_tag (task, valent_ebook_store_init_async);

  e_book_client_connect (self->source,
                         -1,
                         destroy,
                         (GAsyncReadyCallback)e_book_client_connect_cb,
                         g_steal_pointer (&task));
}

static void
g_async_initable_iface_init (GAsyncInitableIface *iface)
{
  iface->init_async = valent_ebook_store_init_async;
}

/*
 * ValentObject
 */
static void
valent_ebook_store_destroy (ValentObject *object)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  if (self->view != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->view, on_objects_added, self);
      g_signal_handlers_disconnect_by_func (self->view, on_objects_removed, self);
      g_clear_object (&self->view);
    }

  VALENT_OBJECT_CLASS (valent_ebook_store_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_ebook_store_finalize (GObject *object)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  g_clear_object (&self->connection);
  g_clear_object (&self->source);
  g_clear_object (&self->view);
  g_clear_object (&self->client);

  G_OBJECT_CLASS (valent_ebook_store_parent_class)->finalize (object);
}

static void
valent_ebook_store_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  switch ((ValentEBookStoreProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    case PROP_SOURCE:
      g_value_set_object (value, self->source);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_ebook_store_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentEBookStore *self = VALENT_EBOOK_STORE (object);

  switch ((ValentEBookStoreProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_set_object (&self->connection, g_value_get_object (value));
      break;

    case PROP_SOURCE:
      g_set_object (&self->source, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_ebook_store_class_init (ValentEBookStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->finalize = valent_ebook_store_finalize;
  object_class->get_property = valent_ebook_store_get_property;
  object_class->set_property = valent_ebook_store_set_property;

  vobject_class->destroy = valent_ebook_store_destroy;

  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         TRACKER_TYPE_SPARQL_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_SOURCE] =
    g_param_spec_object ("source", NULL, NULL,
                         E_TYPE_SOURCE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_ebook_store_init (ValentEBookStore *self)
{
}

