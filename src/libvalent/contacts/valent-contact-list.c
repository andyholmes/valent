// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-list"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <libvalent-core.h>

#include "valent-contacts.h"
#include "valent-eds.h"

#include "valent-contact-list.h"

#define GET_CONTACT_RQ      "/ca/andyholmes/Valent/sparql/get-contact.rq"
#define GET_CONTACT_LIST_RQ "/ca/andyholmes/Valent/sparql/get-contact-list.rq"

/**
 * ValentContactList:
 *
 * An abstract base class for address books.
 *
 * `ValentContactList` is a base class to provide an interface to an address
 * book. This usually means adding, removing and querying contacts.
 *
 * Since: 1.0
 */

struct _ValentContactList
{
  ValentObject             parent_instance;

  TrackerSparqlConnection *connection;
  TrackerNotifier         *notifier;
  TrackerSparqlStatement  *get_contact_stmt;
  TrackerSparqlStatement  *get_contact_list_stmt;
  GRegex                  *iri_pattern;
  char                    *iri;
  GCancellable            *cancellable;

  /* list */
  GSequence               *items;
  unsigned int             last_position;
  GSequenceIter           *last_iter;
  gboolean                 last_position_valid;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

static void   valent_contact_list_load         (ValentContactList *self);
static void   valent_contact_list_load_contact (ValentContactList *self,
                                                const char        *iri);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentContactList, valent_contact_list, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))


typedef enum {
  PROP_CONNECTION = 1,
} ValentContactListProperty;

static GParamSpec *properties[PROP_CONNECTION + 1] = { NULL, };

static inline int
valent_contact_list_lookup_func (gconstpointer a,
                                 gconstpointer b,
                                 gpointer      user_data)
{
  const char *uid = e_contact_get_const ((EContact *)a, E_CONTACT_UID);
  const char *iri = g_strrstr ((const char *)b, uid);

  if (iri == NULL)
    return -1;

  return g_utf8_collate (uid, iri);
}

static void
valent_contact_list_load_contact_cb (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  ValentContactList *self = VALENT_CONTACT_LIST (object);
  g_autoptr (EContact) contact = NULL;
  GSequenceIter *it;
  unsigned int position;
  g_autoptr (GError) error = NULL;

  contact = g_task_propagate_pointer (G_TASK (result), &error);
  if (contact == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          const char *urn = g_task_get_task_data (G_TASK (result));
          g_warning ("%s(): %s: %s", G_STRFUNC, urn, error->message);
        }

      return;
    }

  it = g_sequence_append (self->items, g_object_ref (contact));
  position = g_sequence_iter_get_position (it);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static void
valent_contact_list_remove_contact (ValentContactList *self,
                                    const char        *iri)
{
  GSequenceIter *it;
  unsigned int position;

  g_assert (VALENT_IS_CONTACT_LIST (self));

  it = g_sequence_lookup (self->items,
                          (char *)iri,
                          valent_contact_list_lookup_func,
                          NULL);

  if (it != NULL)
    {
      position = g_sequence_iter_get_position (it);
      g_sequence_remove (it);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    }
}

gboolean
valent_contact_list_event_is_contact (ValentContactList *self,
                                      const char        *iri)
{
  return g_regex_match (self->iri_pattern, iri, G_REGEX_MATCH_DEFAULT, NULL);
}

static void
on_notifier_event (TrackerNotifier   *notifier,
                   const char        *service,
                   const char        *graph,
                   GPtrArray         *events,
                   ValentContactList *self)
{
  g_assert (VALENT_IS_CONTACT_LIST (self));

  if (g_strcmp0 (VALENT_CONTACTS_GRAPH, graph) != 0)
    return;

  for (unsigned int i = 0; i < events->len; i++)
    {
      TrackerNotifierEvent *event = g_ptr_array_index (events, i);
      const char *urn = tracker_notifier_event_get_urn (event);

      if (!valent_contact_list_event_is_contact (self, urn))
        continue;

      switch (tracker_notifier_event_get_event_type (event))
        {
        case TRACKER_NOTIFIER_EVENT_CREATE:
          VALENT_NOTE ("CREATE: %s", urn);
          valent_contact_list_load_contact (self, urn);
          break;

        case TRACKER_NOTIFIER_EVENT_DELETE:
          VALENT_NOTE ("DELETE: %s", urn);
          valent_contact_list_remove_contact (self, urn);
          break;

        case TRACKER_NOTIFIER_EVENT_UPDATE:
          VALENT_NOTE ("UPDATE: %s", urn);
          // valent_contact_list_update_contact (self, urn);
          break;

        default:
          g_warn_if_reached ();
        }
    }
}

#define CURSOR_CONTACT_IRI   (0)
#define CURSOR_CONTACT_UID   (1)
#define CURSOR_CONTACT_VCARD (2)

static EContact *
valent_contact_from_sparql_cursor (TrackerSparqlCursor *cursor)
{
  // const char *iri = NULL;
  const char *uid = NULL;
  const char *vcard = NULL;

  g_assert (TRACKER_IS_SPARQL_CURSOR (cursor));

  if (!tracker_sparql_cursor_is_bound (cursor, CURSOR_CONTACT_UID) ||
      !tracker_sparql_cursor_is_bound (cursor, CURSOR_CONTACT_VCARD))
    g_return_val_if_reached (NULL);

  // iri = tracker_sparql_cursor_get_string (cursor, CURSOR_CONTACT_IRI, NULL);
  uid = tracker_sparql_cursor_get_string (cursor, CURSOR_CONTACT_UID, NULL);
  vcard = tracker_sparql_cursor_get_string (cursor, CURSOR_CONTACT_VCARD, NULL);

  return e_contact_new_from_vcard_with_uid (vcard, uid);
}

static void
cursor_get_contact_cb (TrackerSparqlCursor *cursor,
                       GAsyncResult        *result,
                       gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GError *error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      g_autoptr (EContact) contact = NULL;

      contact = valent_contact_from_sparql_cursor (cursor);
      g_task_return_pointer (task, g_steal_pointer (&contact), g_object_unref);
    }
  else
    {
      if (error == NULL)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to find contact");
        }

      g_task_return_error (task, g_steal_pointer (&error));
    }

  tracker_sparql_cursor_close (cursor);
}

static void
execute_get_contact_cb (TrackerSparqlStatement *stmt,
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
                                    (GAsyncReadyCallback) cursor_get_contact_cb,
                                    g_object_ref (task));
}

static void
valent_contact_list_load_contact (ValentContactList *self,
                                  const char        *iri)
{
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_CONTACT_LIST (self));
  g_assert (iri != NULL);

  task = g_task_new (self, self->cancellable, valent_contact_list_load_contact_cb, NULL);
  g_task_set_source_tag (task, valent_contact_list_load_contact);

  if (self->get_contact_stmt == NULL)
    {
      self->get_contact_stmt =
        tracker_sparql_connection_load_statement_from_gresource (self->connection,
                                                                 GET_CONTACT_RQ,
                                                                 self->cancellable,
                                                                 &error);
    }

  if (self->get_contact_stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_statement_bind_string (self->get_contact_stmt, "iri", iri);
  tracker_sparql_statement_execute_async (self->get_contact_stmt,
                                          self->cancellable,
                                          (GAsyncReadyCallback) execute_get_contact_cb,
                                          g_object_ref (task));
}

static void
cursor_get_contact_list_cb (TrackerSparqlCursor *cursor,
                            GAsyncResult        *result,
                            gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentContactList *self = g_task_get_source_object (task);
  GPtrArray *contacts = g_task_get_task_data (task);
  GError *error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      EContact *contact = NULL;

      contact = valent_contact_from_sparql_cursor (cursor);
      if (contact != NULL)
        g_ptr_array_add (contacts, g_steal_pointer (&contact));

      tracker_sparql_cursor_next_async (cursor,
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback) cursor_get_contact_list_cb,
                                        g_object_ref (task));
      return;
    }

  if (error != NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_warning ("%s(): %s: %s", G_STRFUNC, self->iri, error->message);
    }
  else if (contacts->len > 0)
    {
      unsigned int position = g_sequence_get_length (self->items);

      for (unsigned int i = 0; i < contacts->len; i++)
        {
          g_sequence_append (self->items,
                             g_object_ref (g_ptr_array_index (contacts, i)));
        }

      g_list_model_items_changed (G_LIST_MODEL (self), position, 0, contacts->len);
    }

  g_task_return_boolean (task, TRUE);
  tracker_sparql_cursor_close (cursor);
}

static void
execute_get_contact_list_cb (TrackerSparqlStatement *stmt,
                             GAsyncResult           *result,
                             gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GError *error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    g_task_get_cancellable (G_TASK (result)),
                                    (GAsyncReadyCallback) cursor_get_contact_list_cb,
                                    g_object_ref (task));
}

static void
valent_contact_list_load (ValentContactList *self)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CONTACT_LIST (self));
  g_assert (TRACKER_IS_SPARQL_CONNECTION (self->connection));

  if (self->get_contact_list_stmt == NULL)
    {
      self->get_contact_list_stmt =
        tracker_sparql_connection_load_statement_from_gresource (self->connection,
                                                                 GET_CONTACT_LIST_RQ,
                                                                 self->cancellable,
                                                                 &error);
    }

  if (self->get_contact_list_stmt == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  task = g_task_new (self, self->cancellable, NULL, NULL);
  g_task_set_source_tag (task, valent_contact_list_load);
  g_task_set_task_data (task,
                        g_ptr_array_new_with_free_func (g_object_unref),
                        (GDestroyNotify)g_ptr_array_unref);

  tracker_sparql_statement_bind_string (self->get_contact_list_stmt, "iri",
                                        self->iri);
  tracker_sparql_statement_execute_async (self->get_contact_list_stmt,
                                          g_task_get_cancellable (task),
                                          (GAsyncReadyCallback) execute_get_contact_list_cb,
                                          g_object_ref (task));
}

/*
 * GListModel
 */
static gpointer
valent_contact_list_get_item (GListModel   *list,
                              unsigned int  position)
{
  ValentContactList *self = VALENT_CONTACT_LIST (list);
  GSequenceIter *it = NULL;

  g_assert (VALENT_IS_CONTACT_LIST (self));

  if (self->last_position_valid)
    {
      if (position < G_MAXUINT && self->last_position == position + 1)
        it = g_sequence_iter_prev (self->last_iter);
      else if (position > 0 && self->last_position == position - 1)
        it = g_sequence_iter_next (self->last_iter);
      else if (self->last_position == position)
        it = self->last_iter;
    }

  if (it == NULL)
    it = g_sequence_get_iter_at_pos (self->items, position);

  self->last_iter = it;
  self->last_position = position;
  self->last_position_valid = TRUE;

  if (g_sequence_iter_is_end (it))
    return NULL;

  return g_object_ref (g_sequence_get (it));
}

static GType
valent_contact_list_get_item_type (GListModel *list)
{
  return E_TYPE_CONTACT;
}

static unsigned int
valent_contact_list_get_n_items (GListModel *list)
{
  ValentContactList *self = VALENT_CONTACT_LIST (list);

  g_assert (VALENT_IS_CONTACT_LIST (self));

  return g_sequence_get_length (self->items);
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_contact_list_get_item;
  iface->get_item_type = valent_contact_list_get_item_type;
  iface->get_n_items = valent_contact_list_get_n_items;
}

/*
 * GObject
 */
static void
valent_contact_list_constructed (GObject *object)
{
  ValentContactList *self = VALENT_CONTACT_LIST (object);

  G_OBJECT_CLASS (valent_contact_list_parent_class)->constructed (object);

  self->cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  g_object_get (VALENT_OBJECT (self), "iri", &self->iri, NULL);
  if (self->connection != NULL)
    {
      g_autofree char *iri_pattern = NULL;

      self->notifier = tracker_sparql_connection_create_notifier (self->connection);
      g_signal_connect_object (self->notifier,
                               "events",
                               G_CALLBACK (on_notifier_event),
                               self,
                               G_CONNECT_DEFAULT);

      iri_pattern = g_strdup_printf ("^%s:([^:]+)$", self->iri);
      self->iri_pattern = g_regex_new (iri_pattern,
                                       G_REGEX_OPTIMIZE,
                                       G_REGEX_MATCH_DEFAULT,
                                       NULL);

      valent_contact_list_load (self);
    }
}

static void
valent_contact_list_destroy (ValentObject *object)
{
  ValentContactList *self = VALENT_CONTACT_LIST (object);

  g_signal_handlers_disconnect_by_func (self->notifier, on_notifier_event, self);

  VALENT_OBJECT_CLASS (valent_contact_list_parent_class)->destroy (object);
}

static void
valent_contact_list_finalize (GObject *object)
{
  ValentContactList *self = VALENT_CONTACT_LIST (object);

  g_clear_object (&self->connection);
  g_clear_pointer (&self->iri, g_free);
  g_clear_object (&self->notifier);
  g_clear_object (&self->get_contact_stmt);
  g_clear_object (&self->get_contact_list_stmt);
  g_clear_pointer (&self->iri_pattern, g_regex_unref);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->items, g_sequence_free);

  G_OBJECT_CLASS (valent_contact_list_parent_class)->finalize (object);
}


static void
valent_contact_list_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentContactList *self = VALENT_CONTACT_LIST (object);

  switch ((ValentContactListProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_list_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentContactList *self = VALENT_CONTACT_LIST (object);

  switch ((ValentContactListProperty)prop_id)
    {
    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_list_class_init (ValentContactListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_contact_list_constructed;
  object_class->finalize = valent_contact_list_finalize;
  object_class->get_property = valent_contact_list_get_property;
  object_class->set_property = valent_contact_list_set_property;

  vobject_class->destroy = valent_contact_list_destroy;

  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                          TRACKER_TYPE_SPARQL_CONNECTION,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_contact_list_init (ValentContactList *self)
{
  self->items = g_sequence_new (g_object_unref);
}


