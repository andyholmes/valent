// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-messages-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>
#include <tracker-sparql.h>

#include "valent-message.h"
#include "valent-message-attachment.h"
#include "valent-message-thread.h"

#include "valent-messages-adapter.h"
#include "valent-messages-adapter-private.h"

#define GET_MESSAGE_RQ     "/ca/andyholmes/Valent/sparql/get-message.rq"
#define GET_THREAD_RQ      "/ca/andyholmes/Valent/sparql/get-thread.rq"
#define GET_THREADS_RQ     "/ca/andyholmes/Valent/sparql/get-threads.rq"
#define SEARCH_MESSAGES_RQ "/ca/andyholmes/Valent/sparql/search-messages.rq"


/**
 * ValentMessagesAdapter:
 *
 * An abstract base class for address book providers.
 *
 * `ValentMessagesAdapter` is a base class for plugins that provide an
 * interface to manage messaging (i.e. SMS/MMS). This usually means monitoring
 * and querying [class@Valent.MessagesAdapter] instances.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-MessagesAdapterPriority`
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
  GPtrArray               *threads;

  TrackerSparqlStatement  *get_threads_stmt;
} ValentMessagesAdapterPrivate;

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentMessagesAdapter, valent_messages_adapter, VALENT_TYPE_EXTENSION,
                                  G_ADD_PRIVATE (ValentMessagesAdapter)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

typedef enum
{
  PROP_CONNECTION = 1,
} ValentMessagesAdapterProperty;

static GParamSpec *properties[PROP_CONNECTION + 1] = { 0, };

static gboolean
valent_messages_adapter_open (ValentMessagesAdapter  *self,
                              GError                **error)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  ValentContext *context = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) ontology = NULL;

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

  /* priv->notifier = tracker_sparql_connection_create_notifier (priv->connection); */
  /* g_signal_connect_object (priv->notifier, */
  /*                          "events", */
  /*                          G_CALLBACK (on_notifier_event), */
  /*                          self, */
  /*                          G_CONNECT_DEFAULT); */

  return TRUE;
}

/*
 * GListModel
 */
static gpointer
valent_messages_adapter_get_item (GListModel   *list,
                                  unsigned int  position)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (list);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  ValentMessageThread *thread = NULL;
  g_autofree char *iri = NULL;
  g_autoptr (ValentMessage) latest_message = NULL;
  g_auto (GStrv) participants = NULL;

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));

  if G_UNLIKELY (position >= priv->threads->len)
    return NULL;

  // HACK: return a duplicate thread to avoid accruing memory
  thread = g_ptr_array_index (priv->threads, position);
  g_object_get (thread,
                "iri",            &iri,
                "latest-message", &latest_message,
                "participants",   &participants,
                NULL);

  return g_object_new (VALENT_TYPE_MESSAGE_THREAD,
                       "connection",     priv->connection,
                       "iri",            iri,
                       "latest-message", latest_message,
                       "participants",   participants,
                       NULL);
}

static GType
valent_messages_adapter_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MESSAGE_THREAD;
}

static unsigned int
valent_messages_adapter_get_n_items (GListModel *list)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (list);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));

  return priv->threads->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_messages_adapter_get_item;
  iface->get_item_type = valent_messages_adapter_get_item_type;
  iface->get_n_items = valent_messages_adapter_get_n_items;
}

/*
 * ValentMessagesAdapterPrivate
 *
 */
ValentMessage *
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

static ValentMessageThread *
valent_message_thread_from_sparql_cursor (TrackerSparqlCursor *cursor)
{
  ValentMessage *message = NULL;
  const char *iri = NULL;
  g_autoptr (GListStore) attachments = NULL;
  ValentMessageBox box = VALENT_MESSAGE_BOX_ALL;
  int64_t date = 0;
  g_autoptr (GDateTime) datetime = NULL;
  int64_t message_id;
  gboolean read = FALSE;
  const char *recipients = NULL;
  g_auto (GStrv) recipientv = NULL;
  const char *sender = NULL;
  int64_t subscription_id = -1;
  const char *text = NULL;
  int64_t thread_id = -1;

  g_assert (TRACKER_IS_SPARQL_CURSOR (cursor));

  attachments = g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT);
  box = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_BOX);

  datetime = tracker_sparql_cursor_get_datetime (cursor, CURSOR_MESSAGE_DATE);
  if (datetime != NULL)
    date = g_date_time_to_unix_usec (datetime) / 1000;

  message_id = tracker_sparql_cursor_get_integer (cursor, CURSOR_MESSAGE_ID);
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

  iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_IRI, NULL);
  message = g_object_new (VALENT_TYPE_MESSAGE,
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

  /* Attachment
   */
  if (tracker_sparql_cursor_is_bound (cursor, CURSOR_MESSAGE_ATTACHMENT_IRI /* communicationChannel */))
    iri = tracker_sparql_cursor_get_string (cursor, CURSOR_MESSAGE_ATTACHMENT_IRI, NULL);

  return g_object_new (VALENT_TYPE_MESSAGE_THREAD,
                       "connection",     tracker_sparql_cursor_get_connection (cursor),
                       "iri",            iri,
                       "latest-message", message,
                       NULL);
}

static void
cursor_get_threads_cb (TrackerSparqlCursor *cursor,
                       GAsyncResult        *result,
                       gpointer             user_data)
{
  g_autoptr (ValentMessagesAdapter) self = VALENT_MESSAGES_ADAPTER (g_steal_pointer (&user_data));
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      ValentMessageThread *thread = NULL;

      thread = valent_message_thread_from_sparql_cursor (cursor);
      if (thread != NULL)
        g_ptr_array_add (priv->threads, g_steal_pointer (&thread));

      tracker_sparql_cursor_next_async (cursor,
                                        NULL, // priv->cancellable,
                                        (GAsyncReadyCallback) cursor_get_threads_cb,
                                        g_object_ref (self));
    }
  else
    {
      if (error == NULL)
        g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, priv->threads->len);
      else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      tracker_sparql_cursor_close (cursor);
    }
}

static void
execute_get_threads_cb (TrackerSparqlStatement *stmt,
                        GAsyncResult           *result,
                        gpointer                user_data)
{
  g_autoptr (ValentMessagesAdapter) self = VALENT_MESSAGES_ADAPTER (g_steal_pointer (&user_data));
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
                                    NULL, // priv->cancellable,
                                    (GAsyncReadyCallback) cursor_get_threads_cb,
                                    g_object_ref (self));
}

static void
valent_messages_adapter_get_threads (ValentMessagesAdapter *self)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MESSAGES_ADAPTER (self));

  if (priv->get_threads_stmt == NULL)
    {
      priv->get_threads_stmt =
        tracker_sparql_connection_load_statement_from_gresource (priv->connection,
                                                                 GET_THREADS_RQ,
                                                                 NULL, // priv->cancellable,
                                                                 &error);
    }

  if (priv->get_threads_stmt == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  tracker_sparql_statement_execute_async (priv->get_threads_stmt,
                                          NULL, //priv->cancellable,
                                          (GAsyncReadyCallback) execute_get_threads_cb,
                                          g_object_ref (self));
}

/*
 * ValentMessagesAdapter
 */
/* LCOV_EXCL_START */
static void
valent_messages_adapter_real_send_message (ValentMessagesAdapter *adapter,
                                           ValentMessage         *message,
                                           GCancellable          *cancellable,
                                           GAsyncReadyCallback    callback,
                                           gpointer               user_data)
{
  g_assert (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_assert (VALENT_IS_MESSAGE (message));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (adapter, callback, user_data,
                           valent_messages_adapter_real_send_message,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement send_message",
                           G_OBJECT_TYPE_NAME (adapter));
}

static gboolean
valent_messages_adapter_real_send_message_finish (ValentMessagesAdapter  *adapter,
                                                  GAsyncResult           *result,
                                                  GError                **error)
{
  g_assert (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
valent_messages_adapter_real_export_adapter (ValentMessagesAdapter *adapter,
                                             ValentMessagesAdapter *object)
{
  g_assert (VALENT_MESSAGES_ADAPTER (adapter));
  g_assert (VALENT_MESSAGES_ADAPTER (object));
}

static void
valent_messages_adapter_real_unexport_adapter (ValentMessagesAdapter *adapter,
                                               ValentMessagesAdapter *object)
{
  g_assert (VALENT_MESSAGES_ADAPTER (adapter));
  g_assert (VALENT_MESSAGES_ADAPTER (object));
}
/* LCOV_EXCL_STOP */

/*
 * ValentObject
 */
static void
valent_messages_adapter_destroy (ValentObject *object)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  g_clear_object (&priv->notifier);
  g_clear_object (&priv->get_threads_stmt);

  if (priv->connection != NULL)
    {
      tracker_sparql_connection_close (priv->connection);
      g_clear_object (&priv->connection);
    }

  VALENT_OBJECT_CLASS (valent_messages_adapter_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_messages_adapter_constructed (GObject *object)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);
  g_autoptr (GError) error = NULL;

  G_OBJECT_CLASS (valent_messages_adapter_parent_class)->constructed (object);

  if (priv->connection == NULL)
    {
      if (!valent_messages_adapter_open (self, &error))
        g_error ("%s(): %s", G_STRFUNC, error->message);
    }

  valent_messages_adapter_get_threads (self);
}

static void
valent_messages_adapter_finalize (GObject *object)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  g_clear_object (&priv->connection);
  g_clear_object (&priv->notifier);
  g_clear_pointer (&priv->threads, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_messages_adapter_parent_class)->finalize (object);
}

static void
valent_messages_adapter_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  switch ((ValentMessagesAdapterProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, priv->connection);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_messages_adapter_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentMessagesAdapter *self = VALENT_MESSAGES_ADAPTER (object);
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  switch ((ValentMessagesAdapterProperty)prop_id)
    {
    case PROP_CONNECTION:
      g_assert (priv->connection == NULL);
      priv->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_messages_adapter_class_init (ValentMessagesAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->constructed = valent_messages_adapter_constructed;
  object_class->finalize = valent_messages_adapter_finalize;
  object_class->get_property = valent_messages_adapter_get_property;
  object_class->set_property = valent_messages_adapter_set_property;

  vobject_class->destroy = valent_messages_adapter_destroy;

  klass->send_message = valent_messages_adapter_real_send_message;
  klass->send_message_finish = valent_messages_adapter_real_send_message_finish;
  klass->export_adapter = valent_messages_adapter_real_export_adapter;
  klass->unexport_adapter = valent_messages_adapter_real_unexport_adapter;

  /**
   * ValentMessagesAdapter:connection:
   *
   * The database connection.
   */
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
valent_messages_adapter_init (ValentMessagesAdapter *self)
{
  ValentMessagesAdapterPrivate *priv = valent_messages_adapter_get_instance_private (self);

  priv->threads = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_messages_adapter_send_message: (virtual send_message)
 * @adapter: a `ValentMessagesAdapter`
 * @message: the message to send
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Send @message via @adapter.
 *
 * Call [method@Valent.MessagesAdapter.send_message_finish] to get the result.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_send_message (ValentMessagesAdapter *adapter,
                                      ValentMessage         *message,
                                      GCancellable          *cancellable,
                                      GAsyncReadyCallback    callback,
                                      gpointer               user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MESSAGE (message));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_MESSAGES_ADAPTER_GET_CLASS (adapter)->send_message (adapter,
                                                             message,
                                                             cancellable,
                                                             callback,
                                                             user_data);

  VALENT_EXIT;
}

/**
 * valent_messages_adapter_send_message_finish: (virtual send_message_finish)
 * @adapter: a `ValentMessagesAdapter`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [method@Valent.MessagesAdapter.send_message].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_messages_adapter_send_message_finish (ValentMessagesAdapter  *adapter,
                                             GAsyncResult            *result,
                                             GError                 **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, adapter), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_MESSAGES_ADAPTER_GET_CLASS (adapter)->send_message_finish (adapter,
                                                                          result,
                                                                          error);

  VALENT_RETURN (ret);
}

/**
 * valent_messages_adapter_export_adapter: (virtual export_adapter)
 * @adapter: an `ValentMessagesAdapter`
 * @object: a `ValentMessagesAdapter`
 *
 * Export @object on @adapter.
 *
 * This method is intended to allow device plugins to expose remote message
 * threads to the host system.
 *
 * Implementations must automatically unexport any threads when destroyed.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_export_adapter (ValentMessagesAdapter *adapter,
                                        ValentMessagesAdapter *object)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (object));

  VALENT_MESSAGES_ADAPTER_GET_CLASS (adapter)->export_adapter (adapter,
                                                               object);

  VALENT_EXIT;
}

/**
 * valent_messages_adapter_unexport_adapter: (virtual unexport_adapter)
 * @adapter: an `ValentMessagesAdapter`
 * @object: a `ValentMessagesAdapter`
 *
 * Unexport @object from @adapter.
 *
 * Since: 1.0
 */
void
valent_messages_adapter_unexport_adapter (ValentMessagesAdapter *adapter,
                                          ValentMessagesAdapter *object)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (object));

  VALENT_MESSAGES_ADAPTER_GET_CLASS (adapter)->unexport_adapter (adapter,
                                                                 object);

  VALENT_EXIT;
}

