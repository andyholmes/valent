// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-messages-adapter"

#include "config.h"

#include <inttypes.h>

#include <gio/gio.h>
#include <libtracker-sparql/tracker-sparql.h>
#include <valent.h>

#include "valent-mock-messages-adapter.h"

struct _ValentMockMessagesAdapter
{
  ValentMessagesAdapter  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockMessagesAdapter, valent_mock_messages_adapter, VALENT_TYPE_MESSAGES_ADAPTER)

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
action_callback (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  ValentMockMessagesAdapter *self = VALENT_MOCK_MESSAGES_ADAPTER (user_data);
  const char *name = g_action_get_name (G_ACTION (action));
  int64_t thread_id, message_id;
  g_autoptr (TrackerSparqlConnection) connection = NULL;

  g_variant_get (parameter, "(xx)", &thread_id, &message_id);
  g_object_get (self, "connection", &connection, NULL);
  if (g_str_equal (name, "add-message"))
    {
      g_autofree char *sparql = NULL;

      sparql = g_strdup_printf ("INSERT DATA {"
                                "  GRAPH <valent:messages> {"
                                "    <tel:+1-778-628-3857> rdf:type nco:PhoneNumber ;"
                                "      nco:phoneNumber \"7786283857\" ."
                                ""
                                "    <urn:valent:messages:mock:%"PRId64"> rdf:type vmo:CommunicationChannel ;"
                                "      vmo:communicationChannelId %"PRId64" ;"
                                "      vmo:hasParticipant <tel:+1-778-628-3857> ."
                                ""
                                "    <urn:valent:messages:mock:%"PRId64":%"PRId64"> rdf:type vmo:PhoneMessage ;"
                                "      vmo:communicationChannel <urn:valent:messages:mock:%"PRId64"> ;"
                                "      vmo:subscriptionId -1 ;"
                                "      nmo:plainTextMessageContent \"Sry mistyped the # 😅\" ;"
                                "      nmo:messageSender <tel:+1-778-628-3857> ;"
                                "      vmo:phoneMessageBox vmo:android-message-type-inbox ;"
                                "      nmo:messageFrom <tel:+1-778-628-3857> ;"
                                "      vmo:phoneMessageId %"PRId64" ;"
                                "      nmo:receivedDate \"2018-11-29T17:38:55.320000-08:00\" ;"
                                "      nmo:isRead true ."
                                "  }"
                                "}",
                                thread_id, thread_id,
                                thread_id, message_id,
                                thread_id, message_id);
      tracker_sparql_connection_update_async (connection,
                                              sparql,
                                              NULL,
                                              (GAsyncReadyCallback)update_cb,
                                              NULL);
    }
  else if (g_str_equal (name, "remove-message"))
    {
      g_autofree char *sparql = NULL;

      sparql = g_strdup_printf ("DELETE DATA {"
                                "  GRAPH <valent:messages> {"
                                "    <urn:valent:messages:mock:%"PRId64":%"PRId64"> a vmo:PhoneMessage ;"
                                "  }"
                                "}",
                                thread_id,
                                message_id);
      tracker_sparql_connection_update_async (connection,
                                              sparql,
                                              NULL,
                                              (GAsyncReadyCallback)update_cb,
                                              NULL);
    }
  else if (g_str_equal (name, "remove-list"))
    {
      g_autofree char *sparql = NULL;

      sparql = g_strdup_printf ("DELETE DATA {"
                                "  GRAPH <valent:messages> {"
                                "    vmo:PhoneMessage vmo:communicationChannel <urn:valent:messages:mock:%"PRId64"> ."
                                "    <urn:valent:messages:mock:%"PRId64"> a vmo:CommunicationChannel ."
                                "  }"
                                "}",
                                thread_id,
                                thread_id);
      tracker_sparql_connection_update_async (connection,
                                              sparql,
                                              NULL,
                                              (GAsyncReadyCallback)update_cb,
                                              NULL);
    }
}

static const GActionEntry actions[] = {
    {"add-message",    action_callback, "(xx)", NULL, NULL},
    {"remove-message", action_callback, "(xx)", NULL, NULL},
    {"add-list",       action_callback, "(xx)", NULL, NULL},
    {"remove-list",    action_callback, "(xx)", NULL, NULL},
};

static void
send_message_cb (TrackerSparqlConnection *connection,
                 GAsyncResult            *result,
                 gpointer                 user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GError *error = NULL;

  tracker_sparql_connection_update_finish (connection, result, &error);
  if (error == NULL)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static void
valent_mock_messages_adapter_send_message (ValentMessagesAdapter *adapter,
                                           ValentMessage         *message,
                                           GCancellable          *cancellable,
                                           GAsyncReadyCallback    callback,
                                           gpointer               user_data)
{
  g_autoptr (GTask) task = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autofree char *sparql = NULL;
  int64_t message_id = 0;
  int64_t thread_id = 0;

  g_assert (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_assert (VALENT_IS_MESSAGE (message));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_messages_adapter_send_message);

  g_object_get (adapter, "connection", &connection, NULL);
  message_id = valent_message_get_id (message);
  message_id = valent_message_get_thread_id (message);
  sparql = g_strdup_printf ("INSERT DATA {"
                            "  GRAPH <valent:messages> {"
                            "    <tel:+1-778-628-3857> rdf:type nco:PhoneNumber ;"
                            "      nco:phoneNumber \"7786283857\" ."
                            ""
                            "    <urn:valent:messages:mock:%"PRId64"> rdf:type vmo:CommunicationChannel ;"
                            "      vmo:communicationChannelId %"PRId64" ;"
                            "      vmo:hasParticipant <tel:+1-778-628-3857> ."
                            ""
                            "    <urn:valent:messages:mock:%"PRId64":%"PRId64"> rdf:type vmo:PhoneMessage ;"
                            "      vmo:communicationChannel <urn:valent:messages:mock:%"PRId64"> ;"
                            "      vmo:subscriptionId -1 ;"
                            "      nmo:plainTextMessageContent \"Sry mistyped the # 😅\" ;"
                            "      nmo:messageSender <tel:+1-778-628-3857> ;"
                            "      vmo:phoneMessageBox vmo:android-message-type-sent ;"
                            "      nmo:messageFrom <tel:+1-778-628-3857> ;"
                            "      vmo:phoneMessageId %"PRId64" ;"
                            "      nmo:receivedDate \"2018-11-29T17:34:55.320000-08:00\" ;"
                            "      nmo:isRead true ."
                            "  }"
                            "}",
                            thread_id, thread_id,
                            thread_id, message_id,
                            thread_id, message_id);
  tracker_sparql_connection_update_async (connection,
                                          sparql,
                                          cancellable,
                                          (GAsyncReadyCallback)send_message_cb,
                                          g_steal_pointer (&task));
}

static gboolean
valent_mock_messages_adapter_send_message_finish (ValentMessagesAdapter  *adapter,
                                                  GAsyncResult           *result,
                                                  GError                **error)
{
  g_assert (VALENT_IS_MESSAGES_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/*
 * GObject
 */
static void
valent_mock_messages_adapter_destroy (ValentObject *object)
{
  ValentResource *source;

  VALENT_OBJECT_CLASS (valent_mock_messages_adapter_parent_class)->destroy (object);

  source = valent_resource_get_source (VALENT_RESOURCE (object));
  valent_data_source_clear_cache (VALENT_DATA_SOURCE (source));
}

static void
valent_mock_messages_adapter_constructed (GObject *object)
{
  ValentMockMessagesAdapter *self = VALENT_MOCK_MESSAGES_ADAPTER (object);
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (GInputStream) graph = NULL;
  g_autoptr (GListModel) list = NULL;

  G_OBJECT_CLASS (valent_mock_messages_adapter_parent_class)->constructed (object);

  g_object_get (self, "connection", &connection, NULL);
  graph = g_resources_open_stream ("/plugins/mock/graph-messages.turtle",
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   NULL);
  tracker_sparql_connection_deserialize_async (connection,
                                               TRACKER_DESERIALIZE_FLAGS_NONE,
                                               TRACKER_RDF_FORMAT_TURTLE,
                                               VALENT_MESSAGES_GRAPH,
                                               graph,
                                               NULL,
                                               (GAsyncReadyCallback)deserialize_cb,
                                               NULL);

  while (g_list_model_get_n_items (G_LIST_MODEL (self)) == 0)
    g_main_context_iteration (NULL, FALSE);
}

static void
valent_mock_messages_adapter_class_init (ValentMockMessagesAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentMessagesAdapterClass *adapter_class = VALENT_MESSAGES_ADAPTER_CLASS (klass);

  object_class->constructed = valent_mock_messages_adapter_constructed;

  vobject_class->destroy = valent_mock_messages_adapter_destroy;

  adapter_class->send_message = valent_mock_messages_adapter_send_message;
  adapter_class->send_message_finish = valent_mock_messages_adapter_send_message_finish;
}

static void
valent_mock_messages_adapter_init (ValentMockMessagesAdapter *self)
{
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   self);
}

