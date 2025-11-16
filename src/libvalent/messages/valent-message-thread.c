// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>
// SPDX-FileCopyrightText: Copyright 2015 Alison Karlitskya
// SPDX-FileCopyrightText: Copyright 2015 Lars Uebernickel

#define G_LOG_DOMAIN "valent-message-thread"

#include "config.h"

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <libvalent-core.h>

#include "valent-messages.h"
#include "valent-messages-adapter-private.h"

#include "valent-message-thread.h"

#define GET_MESSAGE_RQ          "/ca/andyholmes/Valent/sparql/get-message.rq"
#define GET_THREAD_MESSAGES_RQ  "/ca/andyholmes/Valent/sparql/get-thread-messages.rq"

struct _ValentMessageThread
{
  ValentObject             parent_instance;

  TrackerSparqlConnection *connection;
  char                    *iri;
  ValentMessage           *latest_message;
  GStrv                    participants;

  TrackerNotifier         *notifier;
  GRegex                  *iri_pattern;
  TrackerSparqlStatement  *get_message_stmt;
  TrackerSparqlStatement  *get_thread_messages_stmt;
  GCancellable            *cancellable;

  /* list */
  GSequence               *items;
  unsigned int             last_position;
  GSequenceIter           *last_iter;
  gboolean                 last_position_valid;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

static void   valent_message_thread_load         (ValentMessageThread *self);
static void   valent_message_thread_load_message (ValentMessageThread *self,
                                                  const char          *iri);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMessageThread, valent_message_thread, VALENT_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

typedef enum {
  PROP_CONNECTION = 1,
  PROP_LATEST_MESSAGE,
  PROP_NOTIFIER,
  PROP_PARTICIPANTS,
} ValentMessageThreadProperty;

static GParamSpec *properties[PROP_PARTICIPANTS + 1] = { NULL, };

static void
valent_message_thread_items_changed (ValentMessageThread *self,
                                     unsigned int         position,
                                     unsigned int         removed,
                                     unsigned int         added)
{
  /* check if the iter cache may have been invalidated */
  if (position <= self->last_position)
    {
      self->last_iter = NULL;
      self->last_position = 0;
      self->last_position_valid = FALSE;
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static inline int
valent_message_thread_sort_func (gconstpointer a,
                                 gconstpointer b,
                                 gpointer      user_data)
{
  int64_t date1 = valent_message_get_date ((ValentMessage *)a);
  int64_t date2 = valent_message_get_date ((ValentMessage *)b);

  return (date1 < date2) ? -1 : (date1 > date2);
}

static void
valent_message_thread_load_message_cb (GObject      *object,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);
  g_autoptr (ValentMessage) message = NULL;
  int64_t latest_date = 0;
  GSequenceIter *it;
  unsigned int position;
  g_autoptr (GError) error = NULL;

  message = g_task_propagate_pointer (G_TASK (result), &error);
  if (message == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          const char *urn = g_task_get_task_data (G_TASK (result));
          g_warning ("%s(): %s: %s", G_STRFUNC, urn, error->message);
        }

      return;
    }

  if (self->latest_message != NULL)
    latest_date = valent_message_get_date (self->latest_message);

  if (valent_message_get_date (message) > latest_date)
    {
      g_set_object (&self->latest_message, message);
      g_object_notify_by_pspec (G_OBJECT (self),
                                properties[PROP_LATEST_MESSAGE]);
    }

  /* Bail if we haven't loaded the rest of the thread yet
   */
  if (self->cancellable == NULL)
    return;

  it = g_sequence_insert_sorted (self->items,
                                 g_object_ref (message),
                                 valent_message_thread_sort_func, NULL);

  position = g_sequence_iter_get_position (it);
  valent_message_thread_items_changed (self, position, 0, 1);
}

static inline int
valent_message_thread_lookup_func (gconstpointer a,
                                   gconstpointer b,
                                   gpointer      user_data)
{
  const char *iri = valent_object_get_iri ((ValentObject *)a);

  return g_utf8_collate (iri, (const char *)b);
}

static void
valent_message_thread_remove_message (ValentMessageThread *self,
                                      const char          *iri)
{
  GSequenceIter *it;
  unsigned int position;

  g_assert (VALENT_IS_MESSAGE_THREAD (self));

  it = g_sequence_lookup (self->items,
                          (char *)iri,
                          valent_message_thread_lookup_func,
                          NULL);

  if (it != NULL)
    {
      position = g_sequence_iter_get_position (it);
      g_sequence_remove (it);
      valent_message_thread_items_changed (self, position, 1, 0);
    }
}

gboolean
valent_message_thread_event_is_message (ValentMessageThread *self,
                                        const char          *iri)
{
  return g_regex_match (self->iri_pattern, iri, G_REGEX_MATCH_DEFAULT, NULL);
}

static void
on_notifier_event (TrackerNotifier     *notifier,
                   const char          *service,
                   const char          *graph,
                   GPtrArray           *events,
                   ValentMessageThread *self)
{
  const char *latest_urn = NULL;

  g_assert (VALENT_IS_MESSAGE_THREAD (self));

  if (g_strcmp0 (VALENT_MESSAGES_GRAPH, graph) != 0)
    return;

  for (unsigned int i = 0; i < events->len; i++)
    {
      TrackerNotifierEvent *event = g_ptr_array_index (events, i);
      const char *urn = tracker_notifier_event_get_urn (event);

      if (!valent_message_thread_event_is_message (self, urn))
        continue;

      switch (tracker_notifier_event_get_event_type (event))
        {
        case TRACKER_NOTIFIER_EVENT_CREATE:
          VALENT_NOTE ("CREATE: %s", urn);
          // HACK: if the thread hasn't been loaded, assume newer messages sort
          //       last and pick one to update the last-message.
          if (self->cancellable != NULL)
            valent_message_thread_load_message (self, urn);
          else if (latest_urn == NULL || g_utf8_collate (latest_urn, urn) < 0)
            latest_urn = urn;
          break;

        case TRACKER_NOTIFIER_EVENT_DELETE:
          VALENT_NOTE ("DELETE: %s", urn);
          valent_message_thread_remove_message (self, urn);
          break;

        case TRACKER_NOTIFIER_EVENT_UPDATE:
          VALENT_NOTE ("UPDATE: %s", urn);
          // valent_message_thread_update_message (self, urn);
          break;

        default:
          g_warn_if_reached ();
        }
    }

  if (latest_urn != NULL)
    valent_message_thread_load_message (self, latest_urn);
}

static void
cursor_get_message_cb (TrackerSparqlCursor *cursor,
                       GAsyncResult        *result,
                       gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  ValentMessage *current = g_task_get_task_data (task);
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      g_autoptr (ValentMessage) message = NULL;

      message = valent_message_from_sparql_cursor (cursor, current);
      if (message != current)
        g_task_set_task_data (task, g_steal_pointer (&message), g_object_unref);

      tracker_sparql_cursor_next_async (cursor,
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback) cursor_get_message_cb,
                                        g_object_ref (task));
      return;
    }

  if (current != NULL)
    {
      g_task_return_pointer (task, g_object_ref (current), g_object_unref);
    }
  else
    {
      if (error == NULL)
        {
          g_set_error_literal (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               "Failed to find message");
        }

      g_task_return_error (task, g_steal_pointer (&error));
    }

  tracker_sparql_cursor_close (cursor);
}

static void
execute_get_message_cb (TrackerSparqlStatement *stmt,
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
                                    (GAsyncReadyCallback) cursor_get_message_cb,
                                    g_object_ref (task));
}

static void
valent_message_thread_load_message (ValentMessageThread *self,
                                    const char          *iri)
{
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_MESSAGE_THREAD (self));
  g_assert (iri != NULL);

  task = g_task_new (self, self->cancellable, valent_message_thread_load_message_cb, NULL);
  g_task_set_source_tag (task, valent_message_thread_load_message);

  if (self->get_message_stmt == NULL)
    {
      self->get_message_stmt =
        tracker_sparql_connection_load_statement_from_gresource (self->connection,
                                                                 GET_MESSAGE_RQ,
                                                                 self->cancellable,
                                                                 &error);
    }

  if (self->get_message_stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_statement_bind_string (self->get_message_stmt, "iri", iri);
  tracker_sparql_statement_execute_async (self->get_message_stmt,
                                          self->cancellable,
                                          (GAsyncReadyCallback) execute_get_message_cb,
                                          g_object_ref (task));
}

static void
valent_message_thread_load_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);
  g_autoptr (GPtrArray) messages = NULL;
  g_autoptr (GError) error = NULL;
  unsigned int position = 0;

  messages = g_task_propagate_pointer (G_TASK (result), &error);
  if (messages == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s: %s", G_STRFUNC, self->iri, error->message);

      return;
    }

  position = g_sequence_get_length (self->items);
  for (unsigned int i = 0; i < messages->len; i++)
    g_sequence_append (self->items, g_object_ref (g_ptr_array_index (messages, i)));

  valent_message_thread_items_changed (self, position, 0, messages->len);
}

static void
cursor_get_messages_cb (TrackerSparqlCursor *cursor,
                        GAsyncResult        *result,
                        gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GPtrArray *messages = g_task_get_task_data (task);
  GError *error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      ValentMessage *current = NULL;
      g_autoptr (ValentMessage) message = NULL;

      if (messages->len > 0)
        current = g_ptr_array_index (messages, messages->len - 1);

      message = valent_message_from_sparql_cursor (cursor, current);
      if (message != current)
        g_ptr_array_add (messages, g_steal_pointer (&message));

      tracker_sparql_cursor_next_async (cursor,
                                        g_task_get_cancellable (task),
                                        (GAsyncReadyCallback) cursor_get_messages_cb,
                                        g_object_ref (task));
    }
  else if (error == NULL)
    {
      g_task_return_pointer (task,
                             g_ptr_array_ref (messages),
                             (GDestroyNotify)g_ptr_array_unref);
      tracker_sparql_cursor_close (cursor);
    }
  else
    {
      g_task_return_error (task, g_steal_pointer (&error));
      tracker_sparql_cursor_close (cursor);
    }
}

static void
execute_get_messages_cb (TrackerSparqlStatement *stmt,
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
                                    (GAsyncReadyCallback) cursor_get_messages_cb,
                                    g_object_ref (task));
}

static void
valent_message_thread_load (ValentMessageThread *self)
{
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_MESSAGE_THREAD (self));

  if (self->connection == NULL || self->cancellable != NULL)
    return;

  self->cancellable = valent_object_ref_cancellable (VALENT_OBJECT (self));
  task = g_task_new (self, self->cancellable, valent_message_thread_load_cb, NULL);
  g_task_set_source_tag (task, valent_message_thread_load);
  g_task_set_task_data (task,
                        g_ptr_array_new_with_free_func (g_object_unref),
                        (GDestroyNotify)g_ptr_array_unref);

  if (self->get_thread_messages_stmt == NULL)
    {
      self->get_thread_messages_stmt =
        tracker_sparql_connection_load_statement_from_gresource (self->connection,
                                                                 GET_THREAD_MESSAGES_RQ,
                                                                 self->cancellable,
                                                                 &error);
    }

  if (self->get_thread_messages_stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_statement_bind_string (self->get_thread_messages_stmt, "iri",
                                        self->iri);
  tracker_sparql_statement_execute_async (self->get_thread_messages_stmt,
                                          g_task_get_cancellable (task),
                                          (GAsyncReadyCallback) execute_get_messages_cb,
                                          g_object_ref (task));
}

/*
 * GListModel
 */
static GType
valent_message_thread_get_item_type (GListModel *model)
{
  return VALENT_TYPE_MESSAGE;
}

static unsigned int
valent_message_thread_get_n_items (GListModel *model)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (model);

  if (self->cancellable == NULL)
    valent_message_thread_load (self);

  return g_sequence_get_length (self->items);
}

static gpointer
valent_message_thread_get_item (GListModel   *model,
                                unsigned int  position)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (model);
  GSequenceIter *it = NULL;

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

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_message_thread_get_item;
  iface->get_item_type = valent_message_thread_get_item_type;
  iface->get_n_items = valent_message_thread_get_n_items;
}

/*
 * GObject
 */
static void
valent_message_thread_constructed (GObject *object)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  G_OBJECT_CLASS (valent_message_thread_parent_class)->constructed (object);

  g_object_get (VALENT_OBJECT (self), "iri", &self->iri, NULL);

  if (self->connection != NULL && self->notifier == NULL)
      self->notifier = tracker_sparql_connection_create_notifier (self->connection);

  if (self->notifier != NULL)
    {
      g_autofree char *iri_pattern = NULL;

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
    }
}

static void
valent_message_thread_destroy (ValentObject *object)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  g_clear_object (&self->get_message_stmt);
  g_clear_object (&self->get_thread_messages_stmt);
  g_clear_pointer (&self->iri_pattern, g_regex_unref);
  g_clear_pointer (&self->iri, g_free);

  if (self->notifier != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->notifier, on_notifier_event, self);
      g_clear_object (&self->notifier);
    }
  g_clear_object (&self->connection);

  VALENT_OBJECT_CLASS (valent_message_thread_parent_class)->destroy (object);
}

static void
valent_message_thread_finalize (GObject *object)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  g_clear_object (&self->latest_message);
  g_clear_pointer (&self->participants, g_strfreev);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->items, g_sequence_free);

  G_OBJECT_CLASS (valent_message_thread_parent_class)->finalize (object);
}

static void
valent_message_thread_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  switch ((ValentMessageThreadProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->connection);
      break;

    case PROP_LATEST_MESSAGE:
      g_value_set_object (value, self->latest_message);
      break;

    case PROP_NOTIFIER:
      g_value_set_object (value, self->notifier);
      break;

    case PROP_PARTICIPANTS:
      g_value_set_boxed (value, self->participants);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_thread_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  switch ((ValentMessageThreadProperty)prop_id)
    {
    case PROP_CONNECTION:
      self->connection = g_value_dup_object (value);
      break;

    case PROP_LATEST_MESSAGE:
      g_assert (self->latest_message == NULL);
      self->latest_message = g_value_dup_object (value);
      break;

    case PROP_NOTIFIER:
      self->notifier = g_value_dup_object (value);
      break;

    case PROP_PARTICIPANTS:
      g_assert (self->participants == NULL);
      self->participants = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_thread_class_init (ValentMessageThreadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_message_thread_constructed;
  object_class->finalize = valent_message_thread_finalize;
  object_class->get_property = valent_message_thread_get_property;
  object_class->set_property = valent_message_thread_set_property;

  vobject_class->destroy = valent_message_thread_destroy;

  /**
   * ValentMessageThread:connection:
   *
   * The [class@Tsparql.SparqlConnection] to the graph.
   */
  properties [PROP_CONNECTION] =
    g_param_spec_object ("connection", NULL, NULL,
                         TRACKER_TYPE_SPARQL_CONNECTION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessageThread:latest-message:
   *
   * The `ValentMessagesAdapter` providing `ValentMessage` objects for the thread.
   */
  properties [PROP_LATEST_MESSAGE] =
    g_param_spec_object ("latest-message", NULL, NULL,
                         VALENT_TYPE_MESSAGE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessageThread:notifier:
   *
   * The [class@Tsparql.Notifier] watching the graph.
   */
  properties [PROP_NOTIFIER] =
    g_param_spec_object ("notifier", NULL, NULL,
                          TRACKER_TYPE_NOTIFIER,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessageThread:participants:
   *
   * A list of contact mediums (i.e. phone number, email) involved in the
   * thread.
   */
  properties [PROP_PARTICIPANTS] =
    g_param_spec_boxed ("participants", NULL, NULL,
                         G_TYPE_STRV,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_message_thread_init (ValentMessageThread *self)
{
  self->items = g_sequence_new (g_object_unref);
}

