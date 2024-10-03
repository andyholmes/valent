// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-ui-utils"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "valent-ui-utils-private.h"

/*< private>
 *
 * Cursor columns for `nco:Contact`.
 */
#define CURSOR_CONTACT_IRI                0
#define CURSOR_CONTACT_UID                1
#define CURSOR_VCARD_DATA                 2

#define SEARCH_CONTACTS_RQ "/ca/andyholmes/Valent/sparql/search-contacts.rq"

/*< private>
 *
 * Cursor columns for `vmo:PhoneMessage`.
 */
#define CURSOR_MESSAGE_IRI                0
#define CURSOR_MESSAGE_BOX                1
#define CURSOR_MESSAGE_DATE               2
#define CURSOR_MESSAGE_ID                 3
#define CURSOR_MESSAGE_READ               4
#define CURSOR_MESSAGE_RECIPIENTS         5
#define CURSOR_MESSAGE_SENDER             6
#define CURSOR_MESSAGE_SUBSCRIPTION_ID    7
#define CURSOR_MESSAGE_TEXT               8
#define CURSOR_MESSAGE_THREAD_ID          9
#define CURSOR_MESSAGE_ATTACHMENT_IRI     10
#define CURSOR_MESSAGE_ATTACHMENT_PREVIEW 11
#define CURSOR_MESSAGE_ATTACHMENT_FILE    12

#define SEARCH_MESSAGES_RQ "/ca/andyholmes/Valent/sparql/search-messages.rq"

G_DEFINE_QUARK (VALENT_CONTACT_PAINTABLE, valent_contact_paintable)


// https://html.spec.whatwg.org/multipage/input.html#valid-e-mail-address
#define EMAIL_PATTERN "[a-zA-Z0-9.!#$%&'*+\\/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*"
// https://mathiasbynens.be/demo/url-regex, @stephenhay, relaxed scheme
#define URI_PATTERN   "\\b([a-zA-Z0-9-]+:[\\/]{1,3}|www[.])[^\\s>]*"
#define COMPILE_FLAGS (G_REGEX_CASELESS | G_REGEX_MULTILINE | G_REGEX_NO_AUTO_CAPTURE | G_REGEX_OPTIMIZE)

static GRegex *email_regex = NULL;
static GRegex *uri_regex = NULL;

static inline gpointer
_g_object_dup0 (gpointer object,
                gpointer user_data)
{
  return object ? g_object_ref ((GObject *)object) : NULL;
}

static gboolean
valent_ui_replace_eval_uri (const GMatchInfo *info,
                            GString          *result,
                            gpointer          user_data)
{
  g_autofree char *uri = NULL;

  uri = g_match_info_fetch (info, 0);

  if (g_uri_is_valid (uri, G_URI_FLAGS_NONE, NULL))
    g_string_append_printf (result, "<a href=\"%s\">%s</a>", uri, uri);
  else if (g_regex_match (email_regex, uri, 0, NULL))
    g_string_append_printf (result, "<a href=\"mailto:%s\">%s</a>", uri, uri);
  else
    g_string_append_printf (result, "<a href=\"https://%s\">%s</a>", uri, uri);

  return FALSE;
}

/**
 * valent_string_to_markup:
 * @text: (nullable): input text
 *
 * Add markup to text for recognized elements.
 *
 * This function currently scans for URLs and e-mail addresses, then amends each
 * element with anchor tags (`<a>`).
 *
 * If @text is %NULL, this function will return %NULL.
 *
 * Returns: (transfer full) (nullable): a string of markup
 *
 * Since: 1.0
 */
char *
valent_string_to_markup (const char *text)
{
  g_autofree char *escaped = NULL;
  g_autofree char *markup = NULL;
  g_autoptr (GError) error = NULL;

  if G_UNLIKELY (text == NULL)
    return NULL;

  if G_UNLIKELY (uri_regex == NULL)
    {
      email_regex = g_regex_new (EMAIL_PATTERN, COMPILE_FLAGS, 0, NULL);
      uri_regex = g_regex_new (URI_PATTERN"|"EMAIL_PATTERN, COMPILE_FLAGS, 0, NULL);
    }

  escaped = g_markup_escape_text (text, -1);
  markup = g_regex_replace_eval (uri_regex,
                                 escaped,
                                 strlen (escaped),
                                 0,
                                 0,
                                 valent_ui_replace_eval_uri,
                                 NULL,
                                 &error);

  return g_steal_pointer (&markup);
}

static GdkPaintable *
_e_contact_get_paintable (EContact  *contact,
                          GError   **error)
{
  g_autoptr (EContactPhoto) photo = NULL;
  GdkPaintable *paintable = NULL;
  GdkTexture *texture = NULL;
  const unsigned char *data;
  size_t len;
  const char *uri;

  g_assert (E_IS_CONTACT (contact));

  paintable = g_object_get_qdata (G_OBJECT (contact),
                                  valent_contact_paintable_quark ());

  if (GDK_IS_PAINTABLE (paintable))
    return paintable;

  photo = e_contact_get (contact, E_CONTACT_PHOTO);
  if (photo == NULL)
    return NULL;

  if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED &&
      (data = e_contact_photo_get_inlined (photo, &len)))
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = g_bytes_new (data, len);
      texture = gdk_texture_new_from_bytes (bytes, NULL);
    }
  else if (photo->type == E_CONTACT_PHOTO_TYPE_URI &&
           (uri = e_contact_photo_get_uri (photo)))
    {
      g_autoptr (GFile) file = NULL;

      file = g_file_new_for_uri (uri);
      texture = gdk_texture_new_from_file (file, NULL);
    }

  if (GDK_IS_PAINTABLE (texture))
    {
      g_object_set_qdata_full (G_OBJECT (contact),
                               valent_contact_paintable_quark (),
                               texture, /* owned */
                               g_object_unref);
    }

  return GDK_PAINTABLE (texture);
}

GdkPaintable *
valent_contact_to_paintable (gpointer  user_data,
                             EContact *contact)
{
  GdkPaintable *paintable = NULL;

  if (contact != NULL)
    paintable = _e_contact_get_paintable (contact, NULL);

  return paintable ? g_object_ref (paintable) : NULL;
}

static EContact *
_e_contact_from_sparql_cursor (TrackerSparqlCursor *cursor)
{
  const char *uid = NULL;
  const char *vcard = NULL;

  g_assert (TRACKER_IS_SPARQL_CURSOR (cursor));

  if (!tracker_sparql_cursor_is_bound (cursor, CURSOR_CONTACT_UID) ||
      !tracker_sparql_cursor_is_bound (cursor, CURSOR_VCARD_DATA))
    g_return_val_if_reached (NULL);

  uid = tracker_sparql_cursor_get_string (cursor, CURSOR_CONTACT_UID, NULL);
  vcard = tracker_sparql_cursor_get_string (cursor, CURSOR_VCARD_DATA, NULL);

  return e_contact_new_from_vcard_with_uid (vcard, uid);
}

static void
cursor_lookup_medium_cb (TrackerSparqlCursor *cursor,
                         GAsyncResult        *result,
                         gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  const char *medium = g_task_get_task_data (task);
  EContact *contact = NULL;
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    contact = _e_contact_from_sparql_cursor (cursor);
  else if (error != NULL)
    g_debug ("%s(): %s", G_STRFUNC, error->message);

  if (contact == NULL)
    {
      g_autoptr (EPhoneNumber) number = NULL;

      contact = e_contact_new ();
      number = e_phone_number_from_string (medium, NULL, NULL);
      if (number != NULL)
        {
          g_autofree char *name = NULL;

          name = e_phone_number_to_string (number,
                                           E_PHONE_NUMBER_FORMAT_NATIONAL);
          e_contact_set (contact, E_CONTACT_FULL_NAME, name);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, medium);
        }
      else
        {
          e_contact_set (contact, E_CONTACT_FULL_NAME, medium);
          if (g_strrstr (medium, "@") != NULL)
            e_contact_set (contact, E_CONTACT_EMAIL_1, medium);
          else
            e_contact_set (contact, E_CONTACT_PHONE_OTHER, medium);
        }
    }

  g_task_return_pointer (task, g_steal_pointer (&contact), g_object_unref);
  tracker_sparql_cursor_close (cursor);
}

static void
execute_lookup_medium_cb (TrackerSparqlStatement *stmt,
                          GAsyncResult           *result,
                          gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GCancellable *cancellable = NULL;
  GError *error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  cancellable = g_task_get_cancellable (G_TASK (result));
  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_lookup_medium_cb,
                                    g_object_ref (task));
}

#define LOOKUP_MEDIUM_FMT                          \
"SELECT ?contact ?uid ?vcardData                   \
WHERE {                                            \
  BIND(IRI(xsd:string(~medium)) AS ?contactMedium) \
  ?contact nco:hasContactMedium ?contactMedium ;   \
           nco:contactUID ?uid ;                   \
           nie:plainTextContent ?vcardData .       \
}                                                  \
LIMIT 1"

/**
 * valent_contacts_adapter_reverse_lookup:
 * @store: a `ValentContactsAdapter`
 * @medium: a contact medium
 * @cancellable: (nullable): `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * A convenience wrapper for finding a contact by phone number or email address.
 *
 * Call [method@Valent.ContactsAdapter.reverse_lookup_finish] to get the result.
 */
void
valent_contacts_adapter_reverse_lookup (ValentContactsAdapter *adapter,
                                        const char            *medium,
                                        GCancellable          *cancellable,
                                        GAsyncReadyCallback    callback,
                                        gpointer               user_data)
{
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (TrackerSparqlStatement) stmt = NULL;
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;
  g_autofree char *medium_iri = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_return_if_fail (medium != NULL && *medium != '\0');
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contacts_adapter_reverse_lookup);
  g_task_set_task_data (task, g_strdup (medium), g_free);

  if (g_strrstr (medium, "@") != NULL)
    {
      medium_iri = g_strdup_printf ("mailto:%s", medium);
    }
  else
    {
      g_autoptr (EPhoneNumber) number = NULL;

      number = e_phone_number_from_string (medium, NULL, NULL);
      if (number != NULL)
        medium_iri = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_RFC3966);
      else
        medium_iri = g_strdup_printf ("tel:%s", medium);
    }

  g_object_get (adapter, "connection", &connection, NULL);
  stmt = tracker_sparql_connection_query_statement (connection,
                                                    LOOKUP_MEDIUM_FMT,
                                                    cancellable,
                                                    &error);

  if (stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      VALENT_EXIT;
    }

  tracker_sparql_statement_bind_string (stmt, "medium", medium_iri);
  tracker_sparql_statement_execute_async (stmt,
                                          cancellable,
                                          (GAsyncReadyCallback) execute_lookup_medium_cb,
                                          g_object_ref (task));

  VALENT_EXIT;
}

/**
 * valent_contacts_adapter_reverse_lookup_finish:
 * @adapter: a `ValentContactsAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [method@Valent.ContactsAdapter.reverse_lookup].
 *
 * Returns: (transfer full): an `EContact`
 */
EContact *
valent_contacts_adapter_reverse_lookup_finish (ValentContactsAdapter  *adapter,
                                               GAsyncResult           *result,
                                               GError                **error)
{
  g_return_val_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
cursor_search_contacts_cb (TrackerSparqlCursor *cursor,
                           GAsyncResult        *result,
                           gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GCancellable *cancellable = g_task_get_cancellable (task);
  GListStore *contacts = g_task_get_task_data (task);
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      g_autoptr (EContact) contact = NULL;

      contact = _e_contact_from_sparql_cursor (cursor);
      if (contact != NULL)
        g_list_store_append (contacts, contact);

      tracker_sparql_cursor_next_async (cursor,
                                        cancellable,
                                        (GAsyncReadyCallback) cursor_search_contacts_cb,
                                        g_object_ref (task));
      return;
    }

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_object_ref (contacts), g_object_unref);

  tracker_sparql_cursor_close (cursor);
}

static void
execute_search_contacts_cb (TrackerSparqlStatement *stmt,
                            GAsyncResult           *result,
                            gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GError *error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_search_contacts_cb,
                                    g_object_ref (task));
}

/**
 * valent_contacts_adapter_search:
 * @adapter: a `ValentContactsAdapter`
 * @query: a string to search for
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Search through all the contacts in @adapter and return the most recent message
 * from each thread containing @query.
 *
 * Call [method@Valent.ContactsAdapter.search_contacts_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_contacts_adapter_search (ValentContactsAdapter *adapter,
                                const char            *query,
                                GCancellable          *cancellable,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data)
{
  g_autoptr (TrackerSparqlStatement) stmt = NULL;
  g_autoptr (GTask) task = NULL;
  g_autofree char *query_sanitized = NULL;
  GError *error = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter));
  g_return_if_fail (query != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_contacts_adapter_search);
  g_task_set_task_data (task, g_list_store_new (E_TYPE_CONTACT), g_object_unref);

  stmt = g_object_dup_data (G_OBJECT (adapter),
                            "valent-contacts-adapter-search",
                            _g_object_dup0,
                            NULL);

  if (stmt == NULL)
    {
      g_autoptr (TrackerSparqlConnection) connection = NULL;

      g_object_get (adapter, "connection", &connection, NULL);
      stmt = tracker_sparql_connection_load_statement_from_gresource (connection,
                                                                      SEARCH_CONTACTS_RQ,
                                                                      cancellable,
                                                                      &error);

      if (stmt == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      g_object_set_data_full (G_OBJECT (adapter),
                              "valent-contacts-adapter-search",
                              g_object_ref (stmt),
                              g_object_unref);
    }

  query_sanitized = tracker_sparql_escape_string (query);
  tracker_sparql_statement_bind_string (stmt, "query", query_sanitized);
  tracker_sparql_statement_execute_async (stmt,
                                          cancellable,
                                          (GAsyncReadyCallback) execute_search_contacts_cb,
                                          g_object_ref (task));

  VALENT_EXIT;
}

/**
 * valent_contacts_adapter_search_finish:
 * @adapter: a `ValentContactsAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [method@Valent.ContactsAdapter.search].
 *
 * Returns: (transfer full) (element-type Valent.Message): a list of contacts
 *
 * Since: 1.0
 */
GListModel *
valent_contacts_adapter_search_finish (ValentContactsAdapter  *adapter,
                                       GAsyncResult           *result,
                                       GError                **error)
{
  GListModel *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_CONTACTS_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

static void
cursor_lookup_thread_cb (TrackerSparqlCursor *cursor,
                         GAsyncResult        *result,
                         gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error) &&
      tracker_sparql_cursor_is_bound (cursor, 0))
    {
      const char *iri = NULL;

      iri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
      g_task_return_pointer (task, g_strdup (iri), g_free);
    }
  else
    {
      if (error == NULL)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to find thread");
        }

      g_task_return_error (task, g_steal_pointer (&error));
    }

  tracker_sparql_cursor_close (cursor);
}

static void
execute_lookup_thread_cb (TrackerSparqlStatement *stmt,
                          GAsyncResult           *result,
                          gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GCancellable *cancellable = NULL;
  GError *error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  cancellable = g_task_get_cancellable (G_TASK (result));
  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_lookup_thread_cb,
                                    g_object_ref (task));
}

#define LOOKUP_THREAD_FMT                                        \
"SELECT DISTINCT ?communicationChannel                           \
WHERE {                                                          \
  VALUES ?specifiedIRIs { %s }                                   \
  ?communicationChannel vmo:hasParticipant ?participant .        \
  FILTER (?participant IN (%s))                                  \
  FILTER NOT EXISTS {                                            \
    ?communicationChannel vmo:hasParticipant ?otherParticipant . \
    FILTER (?otherParticipant NOT IN (%s))                       \
  }                                                              \
}                                                                \
GROUP BY ?communicationChannel                                   \
HAVING (COUNT(DISTINCT ?participant) = %u)"

/**
 * valent_messages_adapter_lookup_thread:
 * @adapter: a `ValentMessagesAdapter`
 * @participants: a list of contact mediums
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Find the thread with @participants.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_lookup_thread (ValentMessagesAdapter *adapter,
                                       const char * const    *participants,
                                       GCancellable          *cancellable,
                                       GAsyncReadyCallback    callback,
                                       gpointer               user_data)
{
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (TrackerSparqlStatement) stmt = NULL;
  g_autoptr (GTask) task = NULL;
  g_autoptr (GStrvBuilder) builder = NULL;
  g_auto (GStrv) iriv = NULL;
  g_autofree char *iris = NULL;
  g_autofree char *values = NULL;
  g_autofree char *sparql = NULL;
  GError *error = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (participants != NULL && participants[0] != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_messages_adapter_lookup_thread);
  g_task_set_task_data (task,
                        g_ptr_array_new_with_free_func (g_object_unref),
                        (GDestroyNotify)g_ptr_array_unref);

  builder = g_strv_builder_new ();
  for (size_t i = 0; participants[i] != NULL; i++)
    {
      g_autofree char *iri = NULL;

      if (g_strrstr (participants[i], "@"))
        {
          iri = g_strdup_printf ("<mailto:%s>", participants[i]);
        }
      else
        {
          g_autoptr (EPhoneNumber) number = NULL;

          number = e_phone_number_from_string (participants[i], NULL, NULL);
          if (number != NULL)
            {
              g_autofree char *uri = NULL;

              uri = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_RFC3966);
              iri = g_strdup_printf ("<%s>", uri);
            }
        }

      if (iri != NULL)
        g_strv_builder_take (builder, g_steal_pointer (&iri));
    }
  iriv = g_strv_builder_end (builder);

  iris = g_strjoinv (", ", iriv);
  values = g_strjoinv (" ", iriv);
  sparql = g_strdup_printf (LOOKUP_THREAD_FMT,
                            values,
                            iris,
                            iris,
                            g_strv_length ((GStrv)iriv));

  g_object_get (adapter, "connection", &connection, NULL);
  stmt = tracker_sparql_connection_query_statement (connection,
                                                    sparql,
                                                    cancellable,
                                                    &error);

  if (stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      VALENT_EXIT;
    }

  tracker_sparql_statement_execute_async (stmt,
                                          cancellable,
                                          (GAsyncReadyCallback) execute_lookup_thread_cb,
                                          g_object_ref (task));

  VALENT_EXIT;
}

/**
 * valent_messages_adapter_lookup_thread_finish:
 * @adapter: a `ValentMessagesAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by valent_contact_adapter_lookup_contact().
 *
 * Returns: (transfer full): an `EContact`
 */
GListModel *
valent_messages_adapter_lookup_thread_finish (ValentMessagesAdapter  *adapter,
                                              GAsyncResult           *result,
                                              GError                **error)
{
  GListModel *ret = NULL;
  g_autofree char *iri = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  iri = g_task_propagate_pointer (G_TASK (result), error);
  if (iri != NULL)
    {
      unsigned int n_threads = g_list_model_get_n_items (G_LIST_MODEL (adapter));

      for (unsigned int i = 0; i < n_threads; i++)
        {
          g_autoptr (GListModel) thread = NULL;
          g_autofree char *thread_iri = NULL;

          thread = g_list_model_get_item (G_LIST_MODEL (adapter), i);
          g_object_get (thread, "iri", &thread_iri, NULL);

          if (g_strcmp0 (iri, thread_iri) == 0)
            {
              ret = g_steal_pointer (&thread);
              break;
            }
        }
    }

  VALENT_RETURN (ret);
}

static ValentMessage *
valent_message_from_sparql_cursor (TrackerSparqlCursor *cursor,
                                   ValentMessage       *current)
{
  ValentMessage *ret = NULL;
  int64_t message_id;

  g_assert (TRACKER_IS_SPARQL_CURSOR (cursor));
  g_assert (current == NULL || VALENT_IS_MESSAGE (current));

  message_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_ID);
  if (current != NULL && valent_message_get_id (current) == message_id)
    {
      ret = g_object_ref (current);
    }
  else
    {
      const char *iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_IRI, NULL);
      g_autoptr (GListStore) attachments = NULL;
      ValentMessageBox box = VALENT_MESSAGE_BOX_ALL;
      int64_t date = 0;
      g_autoptr (GDateTime) datetime = NULL;
      gboolean read = FALSE;
      const char *recipients = NULL;
      g_auto (GStrv) recipientv = NULL;
      const char *sender = NULL;
      int64_t subscription_id = -1;
      const char *text = NULL;
      int64_t thread_id = -1;

      attachments = g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT);
      box = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_BOX);

      datetime = tracker_sparql_cursor_get_datetime (cursor, CURSOR_MESSAGE_DATE);
      if (datetime != NULL)
        date = g_date_time_to_unix_usec (datetime) / 1000;

      read = tracker_sparql_cursor_get_boolean (cursor, CURSOR_MESSAGE_READ);

      recipients = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_RECIPIENTS, NULL);
      if (recipients != NULL)
        recipientv = g_strsplit (recipients, ",", -1);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_SENDER))
        sender = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_SENDER, NULL);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_SUBSCRIPTION_ID))
        subscription_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_SUBSCRIPTION_ID);

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_TEXT))
        text = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_TEXT, NULL);

      thread_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_THREAD_ID);

      ret = g_object_new (VALENT_TYPE_MESSAGE,
                          "iri",             iri,
                          "box",             box,
                          "date",            date,
                          "id",              message_id,
                          "read",            read,
                          "recipients",      recipientv,
                          "sender",          sender,
                          "subscription-id", subscription_id,
                          "text",            text,
                          "thread-id",       thread_id,
                          "attachments",     attachments,
                          NULL);
    }

  /* Attachment
   */
  if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_ATTACHMENT_IRI))
    {
      const char *iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_ATTACHMENT_IRI, NULL);
      GListModel *attachments = valent_message_get_attachments (ret);
      g_autoptr (ValentMessageAttachment) attachment = NULL;
      g_autoptr (GIcon) preview = NULL;
      g_autoptr (GFile) file = NULL;

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_ATTACHMENT_PREVIEW))
        {
          const char *base64_data;

          base64_data = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_ATTACHMENT_PREVIEW, NULL);
          if (base64_data != NULL)
            {
              g_autoptr (GBytes) bytes = NULL;
              unsigned char *data;
              size_t len;

              data = g_base64_decode (base64_data, &len);
              bytes = g_bytes_new_take (g_steal_pointer (&data), len);
              preview = g_bytes_icon_new (bytes);
            }
        }

      if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_ATTACHMENT_FILE))
        {
          const char *file_uri;

          file_uri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_ATTACHMENT_FILE, NULL);
          if (file_uri != NULL)
            file = g_file_new_for_uri (file_uri);
        }

      attachment = g_object_new (VALENT_TYPE_MESSAGE_ATTACHMENT,
                                 "iri",     iri,
                                 "preview", preview,
                                 "file",    file,
                                 NULL);
      g_list_store_append (G_LIST_STORE (attachments), attachment);
    }

  return g_steal_pointer (&ret);
}

static void
cursor_search_messages_cb (TrackerSparqlCursor *cursor,
                           GAsyncResult        *result,
                           gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GListStore *messages = g_task_get_task_data (task);
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      g_autoptr (ValentMessage) current = NULL;
      g_autoptr (ValentMessage) message = NULL;
      unsigned int n_items = 0;

      n_items = g_list_model_get_n_items (G_LIST_MODEL (messages));
      if (n_items > 0)
        current = g_list_model_get_item (G_LIST_MODEL (messages), n_items - 1);

      message = valent_message_from_sparql_cursor (cursor, current);
      if (message != current)
        g_list_store_append (messages, message);

      tracker_sparql_cursor_next_async (cursor,
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback) cursor_search_messages_cb,
                                        g_object_ref (task));
      return;
    }

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_object_ref (messages), g_object_unref);

  tracker_sparql_cursor_close (cursor);
}

static void
execute_search_messages_cb (TrackerSparqlStatement *stmt,
                            GAsyncResult           *result,
                            gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GCancellable *cancellable = g_task_get_cancellable (task);
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GError *error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_search_messages_cb,
                                    g_object_ref (task));
}

/**
 * valent_messages_adapter_search:
 * @adapter: a `ValentMessagesAdapter`
 * @query: a string to search for
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Search through all the messages in @adapter and return the most recent message
 * from each thread containing @query.
 *
 * Call [method@Valent.MessagesAdapter.search_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_search (ValentMessagesAdapter *adapter,
                                const char            *query,
                                GCancellable          *cancellable,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data)
{
  g_autoptr (TrackerSparqlStatement) stmt = NULL;
  g_autoptr (GTask) task = NULL;
  g_autofree char *query_sanitized = NULL;
  GError *error = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (query != NULL);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_messages_adapter_search);
  g_task_set_task_data (task, g_list_store_new (VALENT_TYPE_MESSAGE), g_object_unref);

  stmt = g_object_dup_data (G_OBJECT (adapter),
                            "valent-message-adapter-search",
                            _g_object_dup0,
                            NULL);

  if (stmt == NULL)
    {
      g_autoptr (TrackerSparqlConnection) connection = NULL;

      g_object_get (adapter, "connection", &connection, NULL);
      stmt = tracker_sparql_connection_load_statement_from_gresource (connection,
                                                                      SEARCH_MESSAGES_RQ,
                                                                      cancellable,
                                                                      &error);

      if (stmt == NULL)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          return;
        }

      g_object_set_data_full (G_OBJECT (adapter),
                              "valent-message-adapter-search",
                              g_object_ref (stmt),
                              g_object_unref);
    }

  query_sanitized = tracker_sparql_escape_string (query);
  tracker_sparql_statement_bind_string (stmt, "query", query_sanitized);
  tracker_sparql_statement_execute_async (stmt,
                                          g_task_get_cancellable (task),
                                          (GAsyncReadyCallback) execute_search_messages_cb,
                                          g_object_ref (task));

  VALENT_EXIT;
}

/**
 * valent_messages_adapter_search_finish:
 * @adapter: a `ValentMessagesAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [method@Valent.MessagesAdapter.search].
 *
 * Returns: (transfer full) (element-type Valent.Message): a list of messages
 *
 * Since: 1.0
 */
GListModel *
valent_messages_adapter_search_finish (ValentMessagesAdapter  *adapter,
                                       GAsyncResult           *result,
                                       GError                **error)
{
  GListModel *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter), NULL);
  g_return_val_if_fail (g_task_is_valid (result, adapter), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  VALENT_RETURN (ret);
}

