// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contacts-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libtracker-sparql/tracker-sparql.h>

#include "valent-contacts.h"
#include "valent-contact-list.h"

#include "valent-contacts-adapter.h"

#define GET_CONTACT_RQ       "/ca/andyholmes/Valent/sparql/get-contact.rq"
#define GET_CONTACT_LIST_RQ  "/ca/andyholmes/Valent/sparql/get-contact-list.rq"
#define GET_CONTACT_LISTS_RQ "/ca/andyholmes/Valent/sparql/get-contact-lists.rq"


/**
 * ValentContactsAdapter:
 *
 * An abstract base class for address book providers.
 *
 * `ValentContactsAdapter` is a base class for plugins that provide an
 * interface to address books and contacts. This usually means managing entries
 * in the %VALENT_CONTACTS_GRAPH.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-ContactsAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  TrackerSparqlConnection *connection;
  TrackerNotifier         *notifier;
  TrackerSparqlStatement  *get_contact_list_stmt;
  TrackerSparqlStatement  *get_contact_lists_stmt;
  GRegex                  *iri_pattern;
  char                    *iri;
  GCancellable            *cancellable;

  /* list model */
  GPtrArray               *items;
} ValentContactsAdapterPrivate;

static void   g_list_model_iface_init                   (GListModelInterface   *iface);

static void   valent_contacts_adapter_add_contact_list    (ValentContactsAdapter *self,
                                                           const char            *iri);
static void   valent_contacts_adapter_remove_contact_list (ValentContactsAdapter *self,
                                                           const char            *iri);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentContactsAdapter, valent_contacts_adapter, VALENT_TYPE_EXTENSION,
                                  G_ADD_PRIVATE (ValentContactsAdapter)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

typedef enum
{
  PROP_CONNECTION = 1,
} ValentContactsAdapterProperty;

static GParamSpec *properties[PROP_CONNECTION + 1] = { 0, };

gboolean
valent_contacts_adapter_event_is_contact_list (ValentContactsAdapter *self,
                                               const char            *iri)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  return g_regex_match (priv->iri_pattern, iri, G_REGEX_MATCH_DEFAULT, NULL);
}

static void
on_notifier_event (TrackerNotifier       *notifier,
                   const char            *service,
                   const char            *graph,
                   GPtrArray             *events,
                   ValentContactsAdapter *self)
{
  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));

  if (g_strcmp0 (VALENT_CONTACTS_GRAPH, graph) != 0)
    return;

  for (unsigned int i = 0; i < events->len; i++)
    {
      TrackerNotifierEvent *event = g_ptr_array_index (events, i);
      const char *urn = tracker_notifier_event_get_urn (event);

      if (!valent_contacts_adapter_event_is_contact_list (self, urn))
        continue;

      switch (tracker_notifier_event_get_event_type (event))
        {
        case TRACKER_NOTIFIER_EVENT_CREATE:
          VALENT_NOTE ("CREATE: %s", urn);
          valent_contacts_adapter_add_contact_list (self, urn);
          break;

        case TRACKER_NOTIFIER_EVENT_DELETE:
          VALENT_NOTE ("DELETE: %s", urn);
          valent_contacts_adapter_remove_contact_list (self, urn);
          break;

        case TRACKER_NOTIFIER_EVENT_UPDATE:
          VALENT_NOTE ("UPDATE: %s", urn);
          // valent_contacts_adapter_update_contact_list (self, urn);
          break;

        default:
          g_warn_if_reached ();
        }
    }
}

static ValentContactList *
valent_contact_list_from_sparql_cursor (ValentContactsAdapter *self,
                                        TrackerSparqlCursor *cursor)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);
  const char *iri = NULL;

  if (!tracker_sparql_cursor_is_bound (cursor, 0))
    return NULL;

  iri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  return g_object_new (VALENT_TYPE_CONTACT_LIST,
                       "connection", tracker_sparql_cursor_get_connection (cursor),
                       "notifier",   priv->notifier,
                       "iri",        iri,
                       NULL);
}

static void
cursor_get_contact_lists_cb (TrackerSparqlCursor *cursor,
                             GAsyncResult        *result,
                             gpointer             user_data)
{
  g_autoptr (ValentContactsAdapter) self = VALENT_CONTACTS_ADAPTER (g_steal_pointer (&user_data));
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      ValentContactList *contacts = NULL;

      contacts = valent_contact_list_from_sparql_cursor (self, cursor);
      if (contacts != NULL)
        {
          unsigned int position;

          position = priv->items->len;
          g_ptr_array_add (priv->items, g_object_ref (contacts));
          g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
        }

      tracker_sparql_cursor_next_async (cursor,
                                        g_task_get_cancellable (G_TASK (result)),
                                        (GAsyncReadyCallback) cursor_get_contact_lists_cb,
                                        g_object_ref (self));
    }
  else
    {
      if (error != NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      tracker_sparql_cursor_close (cursor);
    }
}

static void
execute_get_contact_lists_cb (TrackerSparqlStatement *stmt,
                              GAsyncResult           *result,
                              gpointer                user_data)
{
  g_autoptr (ValentContactsAdapter) self = VALENT_CONTACTS_ADAPTER (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  g_autoptr (GError) error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  tracker_sparql_cursor_next_async (cursor,
                                    g_task_get_cancellable (G_TASK (result)),
                                    (GAsyncReadyCallback) cursor_get_contact_lists_cb,
                                    g_object_ref (self));
}

static void
valent_contacts_adapter_load_contact_lists (ValentContactsAdapter *self)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));
  g_return_if_fail (TRACKER_IS_SPARQL_CONNECTION (priv->connection));

  if (priv->cancellable != NULL)
    return;

  priv->cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  if (priv->get_contact_lists_stmt == NULL)
    {
      priv->get_contact_lists_stmt =
        tracker_sparql_connection_load_statement_from_gresource (priv->connection,
                                                                 GET_CONTACT_LISTS_RQ,
                                                                 priv->cancellable,
                                                                 &error);
    }

  if (priv->get_contact_lists_stmt == NULL)
    {
      if (error != NULL && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  tracker_sparql_statement_execute_async (priv->get_contact_lists_stmt,
                                          priv->cancellable,
                                          (GAsyncReadyCallback) execute_get_contact_lists_cb,
                                          g_object_ref (self));
}

static void
valent_contacts_adapter_add_contact_list (ValentContactsAdapter *self,
                                          const char            *iri)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);
  g_autoptr (GListModel) list = NULL;
  unsigned int position = 0;

  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));
  g_assert (TRACKER_IS_SPARQL_CONNECTION (priv->connection));

  list = g_object_new (VALENT_TYPE_CONTACT_LIST,
                       "connection", priv->connection,
                       "notifier",   priv->notifier,
                       "iri",        iri,
                       NULL);

  position = priv->items->len;
  g_ptr_array_add (priv->items, g_steal_pointer (&list));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

static inline gboolean
find_item (gconstpointer a,
           gconstpointer b)
{
  const char *iri = valent_resource_get_iri ((ValentResource *)a);

  return g_strcmp0 (iri, (const char *)b) == 0;
}

static void
valent_contacts_adapter_remove_contact_list (ValentContactsAdapter *self,
                                             const char            *iri)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);
  g_autoptr (GListModel) item = NULL;
  unsigned int position = 0;

  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));

  if (!g_ptr_array_find_with_equal_func (priv->items, iri, find_item, &position))
    {
      g_warning ("No such store \"%s\" found in \"%s\"",
                 iri,
                 G_OBJECT_TYPE_NAME (self));
      return;
    }

  item = g_ptr_array_steal_index (priv->items, position);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

static gboolean
valent_contacts_adapter_open (ValentContactsAdapter  *self,
                              GError                **error)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);
  ValentContext *context = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) ontology = NULL;
  const char *iri = NULL;
  g_autofree char *iri_pattern = NULL;

  context = valent_extension_get_context (VALENT_EXTENSION (self));
  file = valent_context_get_cache_file (context, "metadata");
  ontology = g_file_new_for_uri ("resource:///ca/andyholmes/Valent/ontologies/");

  priv->connection =
    tracker_sparql_connection_new (TRACKER_SPARQL_CONNECTION_FLAGS_NONE,
                                   file,
                                   ontology,
                                   NULL,
                                   error);

  if (priv->connection == NULL)
    return FALSE;

  iri = valent_resource_get_iri (VALENT_RESOURCE (self));
  iri_pattern = g_strdup_printf ("^%s:([^:]+)$", iri);
  priv->iri_pattern = g_regex_new (iri_pattern,
                                   G_REGEX_OPTIMIZE,
                                   G_REGEX_MATCH_DEFAULT,
                                   NULL);

  priv->notifier = tracker_sparql_connection_create_notifier (priv->connection);
  g_signal_connect_object (priv->notifier,
                           "events",
                           G_CALLBACK (on_notifier_event),
                           self,
                           G_CONNECT_DEFAULT);

  return TRUE;
}

/*
 * GListModel
 */
static gpointer
valent_contacts_adapter_get_item (GListModel   *list,
                                  unsigned int  position)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (list);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));

  if G_UNLIKELY (position >= priv->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (priv->items, position));
}

static GType
valent_contacts_adapter_get_item_type (GListModel *list)
{
  return G_TYPE_LIST_MODEL;
}

static unsigned int
valent_contacts_adapter_get_n_items (GListModel *list)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (list);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_assert (VALENT_IS_CONTACTS_ADAPTER (self));

  return priv->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_contacts_adapter_get_item;
  iface->get_item_type = valent_contacts_adapter_get_item_type;
  iface->get_n_items = valent_contacts_adapter_get_n_items;
}

/*
 * ValentObject
 */
static void
valent_contacts_adapter_destroy (ValentObject *object)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_clear_object (&priv->notifier);
  g_clear_object (&priv->get_contact_list_stmt);
  g_clear_object (&priv->get_contact_lists_stmt);
  g_clear_pointer (&priv->iri_pattern, g_regex_unref);
  g_clear_object (&priv->cancellable);

  if (priv->connection != NULL)
    {
      tracker_sparql_connection_close (priv->connection);
      g_clear_object (&priv->connection);
    }

  VALENT_OBJECT_CLASS (valent_contacts_adapter_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_contacts_adapter_constructed (GObject *object)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  g_autoptr (GError) error = NULL;

  G_OBJECT_CLASS (valent_contacts_adapter_parent_class)->constructed (object);

  if (valent_contacts_adapter_open (self, &error))
    valent_contacts_adapter_load_contact_lists (self);
  else
    g_critical ("%s(): %s", G_STRFUNC, error->message);
}

static void
valent_contacts_adapter_finalize (GObject *object)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  g_clear_pointer (&priv->items, g_ptr_array_unref);
  g_clear_object (&priv->cancellable);

  G_OBJECT_CLASS (valent_contacts_adapter_parent_class)->finalize (object);
}

static void
valent_contacts_adapter_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  switch ((ValentContactsAdapterProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contacts_adapter_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentContactsAdapter *self = VALENT_CONTACTS_ADAPTER (object);
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (self);

  switch ((ValentContactsAdapterProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_set_object (&priv->connection, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contacts_adapter_class_init (ValentContactsAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_contacts_adapter_constructed;
  object_class->finalize = valent_contacts_adapter_finalize;
  object_class->get_property = valent_contacts_adapter_get_property;
  object_class->set_property = valent_contacts_adapter_set_property;

  vobject_class->destroy = valent_contacts_adapter_destroy;

  /**
   * ValentContactsAdapter:connection:
   *
   * The database connection.
   */
  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         TRACKER_TYPE_SPARQL_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_contacts_adapter_init (ValentContactsAdapter *adapter)
{
  ValentContactsAdapterPrivate *priv = valent_contacts_adapter_get_instance_private (adapter);

  priv->items = g_ptr_array_new_with_free_func (g_object_unref);
}

