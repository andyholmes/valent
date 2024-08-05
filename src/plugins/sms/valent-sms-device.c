// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-device"

#include "config.h"

#include <inttypes.h>

#include <gio/gio.h>
#include <tracker-sparql.h>
#include <valent.h>

#include "valent-sms-device.h"

#define GET_TIMESTAMP_RQ  "/ca/andyholmes/Valent/sparql/get-timestamp.rq"

struct _ValentSmsDevice
{
  ValentMessagesAdapter    parent_instance;

  ValentDevice            *device;
  TrackerSparqlConnection *connection;
  TrackerSparqlStatement  *get_timestamp_stmt;

  GCancellable            *cancellable;
  GPtrArray               *message_requests;
  GQueue                   attachment_requests;
};

static void   attachment_request_queue                (ValentSmsDevice *self,
                                                       const char      *iri,
                                                       int64_t          part_id,
                                                       const char      *unique_identifier);
static void   valent_sms_device_request_attachment    (ValentSmsDevice *self,
                                                       int64_t          thread_id,
                                                       const char      *unique_identifier);
static void   valent_sms_device_request_conversation  (ValentSmsDevice *self,
                                                       int64_t          thread_id,
                                                       int64_t          range_start_timestamp,
                                                       int64_t          number_to_request);

G_DEFINE_FINAL_TYPE (ValentSmsDevice, valent_sms_device, VALENT_TYPE_MESSAGES_ADAPTER)

typedef enum
{
  PROP_DEVICE = 1,
} ValentSmsDeviceProperty;

static GParamSpec *properties[PROP_DEVICE + 1] = { 0, };


typedef struct
{
  char *iri;
  int64_t part_id;
  char *unique_identifier;
} AttachmentRequest;

static void  attachment_request_next  (ValentSmsDevice *self);

static void
attachment_request_free (gpointer data)
{
  AttachmentRequest *request = data;

  g_clear_pointer (&request->iri, g_free);
  g_clear_pointer (&request->unique_identifier, g_free);
  g_free (request);
}

static void
attachment_request_next_cb (GFile        *file,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr (ValentSmsDevice) self = VALENT_SMS_DEVICE (g_steal_pointer (&user_data));
  g_autoptr (GFileInfo) info = NULL;
  g_autoptr (GError) error = NULL;

  info = g_file_query_info_finish (file, result, &error);
  if (info == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          AttachmentRequest *request;

          request = g_queue_peek_head (&self->attachment_requests);
          valent_sms_device_request_attachment (self,
                                                request->part_id,
                                                request->unique_identifier);
          return;
        }

      g_warning ("%s(): %s", G_STRFUNC, error->message);
    }

  attachment_request_free (g_queue_pop_head (&self->attachment_requests));
  attachment_request_next (self);
}

static void
attachment_request_next (ValentSmsDevice *self)
{
  g_assert (VALENT_IS_SMS_DEVICE (self));

  if (!g_queue_is_empty (&self->attachment_requests))
    {
      AttachmentRequest *request;
      ValentContext *context;
      g_autoptr (GFile) file = NULL;

      request = g_queue_peek_head (&self->attachment_requests);
      context = valent_extension_get_context (VALENT_EXTENSION (self));
      file = valent_context_get_cache_file (context, request->unique_identifier);
      g_file_query_info_async (file,
                               G_FILE_ATTRIBUTE_STANDARD_TYPE,
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               self->cancellable,
                               (GAsyncReadyCallback) attachment_request_next_cb,
                               g_object_ref (self));
    }
}

static void
attachment_request_queue (ValentSmsDevice *self,
                          const char      *iri,
                          int64_t          part_id,
                          const char      *unique_identifier)
{
  AttachmentRequest *request;
  gboolean start = g_queue_is_empty (&self->attachment_requests);

  request = g_new0 (AttachmentRequest, 1);
  request->iri = g_strdup (iri);
  request->part_id = part_id;
  request->unique_identifier = g_strdup (unique_identifier);
  g_queue_push_tail (&self->attachment_requests, g_steal_pointer (&request));

  if (start)
    attachment_request_next (self);
}

static void
update_attachment_cb (TrackerSparqlConnection *connection,
                      GAsyncResult            *result,
                      gpointer                 user_data)
{
  g_autoptr (GError) error = NULL;

  if (!tracker_sparql_connection_update_resource_finish (connection, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("%s(): %s", G_STRFUNC, error->message);
    }
}

static void
attachment_request_query_cb (GFile        *file,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr (ValentSmsDevice) self = g_steal_pointer (&user_data);
  const char *iri = NULL;
  g_autofree char *uri = NULL;
  g_autoptr (TrackerResource) attachment = NULL;
  g_autoptr (GDateTime) accessed = NULL;
  g_autoptr (GDateTime) created = NULL;
  g_autoptr (GDateTime) modified = NULL;
  g_autoptr (GFileInfo) info = NULL;
  g_autoptr (GError) error = NULL;

  info = g_file_query_info_finish (file, result, &error);
  if (info == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  iri = g_object_get_data (G_OBJECT (file), "valent-message-attachment-iri");
  uri = g_file_get_uri (file);

  attachment = tracker_resource_new (iri);
  tracker_resource_set_uri (attachment, "rdf:type", "nfo:Attachment");
  tracker_resource_set_string (attachment, "nie:url", uri);
  tracker_resource_set_string (attachment,
                               "nfo:fileName", g_file_info_get_name (info));
  tracker_resource_set_int64 (attachment,
                              "nfo:fileSize", g_file_info_get_size (info));

  created = g_file_info_get_creation_date_time (info);
  if (created != NULL)
    tracker_resource_set_datetime (attachment, "nfo:fileCreated", created);

  accessed = g_file_info_get_access_date_time (info);
  if (accessed != NULL)
    tracker_resource_set_datetime (attachment, "nfo:fileLastAccessed", accessed);

  modified = g_file_info_get_modification_date_time (info);
  if (modified != NULL)
    tracker_resource_set_datetime (attachment, "nfo:fileLastModified", modified);

  tracker_sparql_connection_update_resource_async (self->connection,
                                                   VALENT_MESSAGES_GRAPH,
                                                   attachment,
                                                   NULL,
                                                   (GAsyncReadyCallback)update_attachment_cb,
                                                   NULL);
}

static void
handle_attachment_file_cb (ValentTransfer *transfer,
                           GAsyncResult   *result,
                           gpointer        user_data)
{
  g_autoptr (ValentSmsDevice) self = g_steal_pointer (&user_data);
  g_autoptr (GError) error = NULL;

  if (!valent_transfer_execute_finish (transfer, result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("%s(): %s", G_STRFUNC, error->message);
    }
  else
    {
      AttachmentRequest *request = g_queue_peek_head (&self->attachment_requests);
      g_autoptr (GFile) file = NULL;

      file = valent_device_transfer_ref_file (VALENT_DEVICE_TRANSFER (transfer));
      g_object_set_data_full (G_OBJECT (file),
                              "valent-message-attachment-iri",
                              g_strdup (request->iri),
                              g_free);
      g_file_query_info_async (file,
                               "standard::*",
                               G_FILE_QUERY_INFO_NONE,
                               G_PRIORITY_DEFAULT,
                               NULL, // cancellable,
                               (GAsyncReadyCallback)attachment_request_query_cb,
                               g_object_ref (self));
    }

  if (!g_queue_is_empty (&self->attachment_requests))
    {
      attachment_request_free (g_queue_pop_head (&self->attachment_requests));
      attachment_request_next (self);
    }
}


static inline TrackerResource *
valent_message_resource_from_json (ValentSmsDevice *self,
                                   JsonNode        *root)
{
  ValentContext *context;
  TrackerResource *message;
  TrackerResource *thread;
  TrackerResource *box;
  JsonNode *node;
  JsonObject *object;
  g_autoptr (GDateTime) datetime = NULL;
  g_autofree char *thread_iri = NULL;
  g_autofree char *iri = NULL;
  int64_t date;
  int64_t message_id;
  int64_t message_type;
  gboolean read;
  const char *sender = NULL;
  int64_t sub_id;
  const char *text = NULL;
  int64_t thread_id;

  g_return_val_if_fail (JSON_NODE_HOLDS_OBJECT (root), NULL);

  /* Check all the required fields exist
   */
  object = json_node_get_object (root);
  node = json_object_get_member (object, "thread_id");
  if (node == NULL || json_node_get_value_type (node) != G_TYPE_INT64)
    {
      g_warning ("%s(): expected \"thread_id\" field holding an integer",
                 G_STRFUNC);
      return NULL;
    }
  thread_id = json_node_get_int (node);

  node = json_object_get_member (object, "_id");
  if (node == NULL || json_node_get_value_type (node) != G_TYPE_INT64)
    {
      g_warning ("%s(): expected \"_id\" field holding an integer", G_STRFUNC);
      return NULL;
    }
  message_id = json_node_get_int (node);

  node = json_object_get_member (object, "date");
  if (node == NULL || json_node_get_value_type (node) != G_TYPE_INT64)
    {
      g_warning ("%s(): expected \"date\" field holding an integer", G_STRFUNC);
      return NULL;
    }
  date = json_node_get_int (node);

  node = json_object_get_member (object, "type");
  if (node == NULL || json_node_get_value_type (node) != G_TYPE_INT64)
    {
      g_warning ("%s(): expected \"type\" field holding an integer", G_STRFUNC);
      return NULL;
    }
  message_type = json_node_get_int (node);

  /* CommunicationChannel
   */
  context = valent_extension_get_context (VALENT_EXTENSION (self));
  thread_iri = g_strdup_printf ("valent://%s/%"PRId64,
                                valent_context_get_path (context),
                                thread_id);
  thread = tracker_resource_new (thread_iri);
  tracker_resource_set_uri (thread, "rdf:type", "vmo:CommunicationChannel");
  tracker_resource_set_int64 (thread, "vmo:communicationChannelId", thread_id);

  /* PhoneMessage
   */
  iri = g_strdup_printf ("%s/%"PRId64, thread_iri, message_id);
  message = tracker_resource_new (iri);
  tracker_resource_set_uri (message, "rdf:type", "vmo:PhoneMessage");
  tracker_resource_set_int64 (message, "vmo:phoneMessageId", message_id);

  datetime = g_date_time_new_from_unix_local_usec (date * 1000);
  if (message_type == VALENT_MESSAGE_BOX_SENT)
    tracker_resource_set_datetime (message, "nmo:sentDate", datetime);
  else if (message_type == VALENT_MESSAGE_BOX_INBOX)
    tracker_resource_set_datetime (message, "nmo:receivedDate", datetime);

  read = !!json_object_get_int_member_with_default (object, "read", 0);
  tracker_resource_set_boolean (message, "nmo:isRead", read);

  text = json_object_get_string_member_with_default (object, "body", NULL);
  if (text != NULL && *text != '\0')
    tracker_resource_set_string (message, "nmo:plainTextMessageContent", text);

  switch (message_type)
    {
    case VALENT_MESSAGE_BOX_ALL:
      box = tracker_resource_new ("vmo:android-message-type-all");
      break;

    case VALENT_MESSAGE_BOX_INBOX:
      box = tracker_resource_new ("vmo:android-message-type-inbox");
      break;

    case VALENT_MESSAGE_BOX_SENT:
      box = tracker_resource_new ("vmo:android-message-type-sent");
      break;

    case VALENT_MESSAGE_BOX_DRAFTS:
      box = tracker_resource_new ("vmo:android-message-type-drafts");
      break;

    case VALENT_MESSAGE_BOX_OUTBOX:
      box = tracker_resource_new ("vmo:android-message-type-outbox");
      break;

    case VALENT_MESSAGE_BOX_FAILED:
      box = tracker_resource_new ("vmo:android-message-type-failed");
      break;

    case VALENT_MESSAGE_BOX_QUEUED:
      box = tracker_resource_new ("vmo:android-message-type-queued");
      break;

    default:
      box = tracker_resource_new ("vmo:android-message-type-all");
      g_warn_if_reached ();
      break;
    }
  tracker_resource_add_take_relation (message,
                                      "vmo:phoneMessageBox",
                                      g_steal_pointer (&box));

  sub_id = json_object_get_int_member_with_default (object, "sub_id", -1);
  tracker_resource_set_int64 (message, "vmo:subscriptionId", sub_id);

/* This is an inferred data point from kdeconnect-android, with the bits 0x1
 * set if the content type is `text/plain` and 0x2 if the message has more than
 * two participants (0x0 if neither were true).
 */
#if 0
  int64_t event = json_object_get_int_member_with_default (object, "event", 0);
#endif

  node = json_object_get_member (object, "addresses");
  if (node != NULL && JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *addresses = json_node_get_array (node);
      unsigned int n_addresses = json_array_get_length (addresses);

      for (unsigned int i = 0; i < n_addresses; i++)
        {
          JsonObject *address = json_array_get_object_element (addresses, i);
          g_autoptr (TrackerResource) medium = NULL;
          g_autofree char *medium_iri = NULL;
          const char *address_str;

          address_str = json_object_get_string_member (address, "address");
          if (address_str == NULL || *address_str == '\0')
            continue;

          /* Sometimes the sender's address is duplicated in the remainder of
           * the list, which is reserved for recipients.
           */
          if (g_strcmp0 (sender, address_str) == 0)
            {
              VALENT_NOTE ("skipping duplicate contact medium \"%s\"", sender);
              continue;
            }

          /* Messages may be sent to or from email addresses.
           */
          if (g_strrstr (address_str, "@"))
            {
              medium_iri = g_strdup_printf ("mailto:%s", address_str);
              medium = tracker_resource_new (medium_iri);
              tracker_resource_set_uri (medium, "rdf:type", "nco:EmailAddress");
              tracker_resource_set_string (medium, "nco:emailAddress", address_str);
            }
          else
            {
              g_autoptr (EPhoneNumber) number = NULL;

              number = e_phone_number_from_string (address_str, NULL, NULL);
              if (number == NULL)
                {
                  VALENT_NOTE ("invalid phone number \"%s\"", address_str);
                  continue;
                }

              medium_iri = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_RFC3966);
              medium = tracker_resource_new (medium_iri);
              tracker_resource_set_uri (medium, "rdf:type", "nco:PhoneNumber");
              tracker_resource_set_string (medium, "nco:phoneNumber", address_str);
            }

          /* If the message is incoming, the first address is the sender. Mark
           * the sender in case it is duplicated in the recipients.
           */
          if (i == 0 && message_type == VALENT_MESSAGE_BOX_INBOX)
            {
              sender = address_str;
              tracker_resource_add_relation (message,
                                             "nmo:messageFrom",
                                             medium);
              tracker_resource_add_relation (message,
                                             "nmo:messageSender",
                                             medium);
            }
          else
            {
              tracker_resource_add_relation (message,
                                             "nmo:primaryMessageRecipient",
                                             medium);
            }

          // TODO: does this result in an exclusive set?
          tracker_resource_add_take_relation (thread,
                                              "vmo:hasParticipant",
                                              g_steal_pointer (&medium));
        }
    }
  tracker_resource_set_take_relation (message,
                                      "vmo:communicationChannel",
                                      g_steal_pointer (&thread));

  node = json_object_get_member (object, "attachments");
  if (node != NULL && JSON_NODE_HOLDS_ARRAY (node))
    {
      JsonArray *attachments = json_node_get_array (node);
      unsigned int n_attachments = json_array_get_length (attachments);

      for (unsigned int i = 0; i < n_attachments; i++)
        {
          JsonObject *attachment = json_array_get_object_element (attachments, i);
          JsonNode *subnode;
          TrackerResource *rel;
          g_autofree char *rel_iri = NULL;
          int64_t part_id = 0;
          const char *unique_identifier = NULL;

          /* NOTE: `part_id` and `mime_type` are not stored in the graph.
           */
          subnode = json_object_get_member (attachment, "part_id");
          if (subnode == NULL || json_node_get_value_type (subnode) != G_TYPE_INT64)
            continue;

          part_id = json_node_get_int (subnode);

          subnode = json_object_get_member (attachment, "unique_identifier");
          if (subnode == NULL || json_node_get_value_type (subnode) != G_TYPE_STRING)
            continue;

          unique_identifier = json_node_get_string (subnode);

          rel_iri = g_strdup_printf ("%s/%s", iri, unique_identifier);
          rel = tracker_resource_new (rel_iri);
          tracker_resource_set_uri (rel, "rdf:type", "nfo:Attachment");
          tracker_resource_set_string (rel, "nfo:fileName", unique_identifier);

          subnode = json_object_get_member (attachment, "encoded_thumbnail");
          if (subnode != NULL && json_node_get_value_type (subnode) == G_TYPE_STRING)
            {
              const char *encoded_thumbnail = NULL;

              encoded_thumbnail = json_node_get_string (subnode);
              tracker_resource_set_string (rel, "vmo:encoded_thumbnail", encoded_thumbnail);
            }

          tracker_resource_add_take_relation (message,
                                              "nmo:hasAttachment",
                                              g_steal_pointer (&rel));
          attachment_request_queue (self, rel_iri, part_id, unique_identifier);
        }
    }

  return message;
}

static void
execute_add_messages_cb (TrackerBatch *batch,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GError *error = NULL;

  if (!tracker_batch_execute_finish (batch, result, &error) &&
      !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      g_debug ("%s(): %s", G_STRFUNC, error->message);
    }
}

static void
valent_sms_device_add_json (ValentSmsDevice *self,
                            JsonNode        *messages)
{
  g_autoptr (TrackerBatch) batch = NULL;
  JsonArray *messages_;
  unsigned int n_messages;

  g_assert (VALENT_IS_SMS_DEVICE (self));
  g_assert (JSON_NODE_HOLDS_ARRAY (messages));

  batch = tracker_sparql_connection_create_batch (self->connection);

  messages_ = json_node_get_array (messages);
  n_messages = json_array_get_length (messages_);
  for (unsigned int i = 0; i < n_messages; i++)
    {
      JsonNode *message = json_array_get_element (messages_, i);
      g_autoptr (TrackerResource) resource = NULL;

      resource = valent_message_resource_from_json (self, message);
      tracker_batch_add_resource (batch, VALENT_MESSAGES_GRAPH, resource);
    }

  tracker_batch_execute_async (batch,
                               self->cancellable,
                               (GAsyncReadyCallback) execute_add_messages_cb,
                               NULL);
}

static void
valent_device_send_packet_cb (ValentDevice *device,
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

/*< private >
 * @self: a `ValentSmsDevice`
 * @part_id: the MMS part ID
 * @unique_identifier: the attachment identifier
 *
 * Send a request for messages starting at @range_start_timestamp in
 * oldest-to-newest order, for a maximum of @number_to_request.
 */
static void
valent_sms_device_request_attachment (ValentSmsDevice *self,
                                      int64_t          part_id,
                                      const char      *unique_identifier)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_DEVICE (self));
  g_return_if_fail (part_id >= 0);
  g_return_if_fail (unique_identifier != NULL && *unique_identifier != '\0');

  valent_packet_init (&builder, "kdeconnect.sms.request_attachment");
  json_builder_set_member_name (builder, "part_id");
  json_builder_add_int_value (builder, part_id);
  json_builder_set_member_name (builder, "unique_identifier");
  json_builder_add_string_value (builder, unique_identifier);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             NULL,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}

/*< private >
 * @self: a `ValentSmsDevice`
 * @range_start_timestamp: the timestamp of the newest message to request
 * @number_to_request: the maximum number of messages to return
 *
 * Send a request for messages starting at @range_start_timestamp in
 * oldest-to-newest order, for a maximum of @number_to_request.
 */
static void
valent_sms_device_request_conversation (ValentSmsDevice *self,
                                        int64_t          thread_id,
                                        int64_t          range_start_timestamp,
                                        int64_t          number_to_request)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_DEVICE (self));
  g_return_if_fail (thread_id >= 0);

  valent_packet_init (&builder, "kdeconnect.sms.request_conversation");
  json_builder_set_member_name (builder, "threadID");
  json_builder_add_int_value (builder, thread_id);

  if (range_start_timestamp > 0)
    {
      json_builder_set_member_name (builder, "rangeStartTimestamp");
      json_builder_add_int_value (builder, range_start_timestamp);
    }

  if (number_to_request > 0)
    {
      json_builder_set_member_name (builder, "numberToRequest");
      json_builder_add_int_value (builder, number_to_request);
    }

  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             NULL,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}

#if 0
static inline void
valent_sms_device_request_conversations (ValentSmsDevice *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_DEVICE (self));

  valent_packet_init (&builder, "kdeconnect.sms.request_conversations");
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device,
                             packet,
                             NULL,
                             (GAsyncReadyCallback) valent_device_send_packet_cb,
                             NULL);
}
#endif

/*
 * ValentMessagesAdapter
 */
static JsonNode *
valent_message_to_packet (ValentMessage  *message,
                          GError        **error)
{
  g_autoptr (JsonBuilder) builder = NULL;
  const char * const *recipients = NULL;
  GListModel *attachments = NULL;
  unsigned int n_attachments = 0;;
  int64_t sub_id = -1;
  const char *text;

  g_return_val_if_fail (VALENT_IS_MESSAGE (message), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  attachments = valent_message_get_attachments (message);
  if (attachments != NULL)
    n_attachments = g_list_model_get_n_items (attachments);

  recipients = valent_message_get_recipients (message);
  if (recipients == NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "message has no recipients");
      return NULL;
    }

  // Build the packet
  valent_packet_init (&builder, "kdeconnect.sms.request");

  json_builder_set_member_name (builder, "addresses");
  json_builder_begin_array (builder);
  for (size_t i = 0; recipients[i] != NULL; i++)
    {
      json_builder_begin_object (builder);
      json_builder_set_member_name (builder, "address");
      json_builder_add_string_value (builder, recipients[i]);
      json_builder_end_object (builder);
    }
  json_builder_end_array (builder);

  text = valent_message_get_text (message);
  json_builder_set_member_name (builder, "messageBody");
  json_builder_add_string_value (builder, text);

  json_builder_set_member_name (builder, "attachments");
  json_builder_begin_array (builder);
  for (unsigned int i = 0; i < n_attachments; i++)
    {
      g_autoptr (ValentMessageAttachment) attachment = NULL;
      GFile *file;
      g_autofree char *basename = NULL;
      g_autofree char *mimetype = NULL;
      g_autofree unsigned char *data = NULL;
      size_t len;
      g_autofree char *encoded_file = NULL;
      g_autoptr (GError) warn = NULL;

      attachment = g_list_model_get_item (attachments, i);
      file = valent_message_attachment_get_file (attachment);
      basename = g_file_get_basename (file);

      // FIXME: async
      if (!g_file_load_contents (file, NULL, (char **)&data, &len, NULL, &warn))
        {
          g_debug ("Failed to load attachment \"%s\"", basename);
          continue;
        }

      encoded_file = g_base64_encode (data, len);
      mimetype = g_content_type_guess (basename, data, len, NULL /* uncertain */);

      json_builder_begin_object (builder);
      json_builder_set_member_name (builder, "fileName");
      json_builder_add_string_value (builder, basename);
      json_builder_set_member_name (builder, "mimeType");
      json_builder_add_string_value (builder, mimetype);
      json_builder_set_member_name (builder, "base64EncodedFile");
      json_builder_add_string_value (builder, encoded_file);
      json_builder_end_object (builder);
    }
  json_builder_end_array (builder);

  sub_id = valent_message_get_subscription_id (message);
  json_builder_set_member_name (builder, "subID");
  json_builder_add_int_value (builder, sub_id);

  json_builder_set_member_name (builder, "version");
  json_builder_add_int_value (builder, 2);

  return valent_packet_end (&builder);
}

static void
valent_sms_device_send_message_cb (ValentDevice *device,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  GError *error = NULL;

  if (valent_device_send_packet_finish (device, result, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static void
valent_sms_device_send_message (ValentMessagesAdapter *adapter,
                                ValentMessage         *message,
                                GCancellable          *cancellable,
                                GAsyncReadyCallback    callback,
                                gpointer               user_data)
{
  ValentSmsDevice *self = VALENT_SMS_DEVICE (adapter);
  g_autoptr (GTask) task = NULL;
  g_autoptr (JsonNode) packet = NULL;
  GError *error = NULL;

  g_assert (VALENT_IS_SMS_DEVICE (self));
  g_assert (VALENT_IS_MESSAGE (message));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  packet = valent_message_to_packet (message, &error);
  if (packet == NULL)
    {
      g_task_report_error (adapter, callback, user_data,
                           valent_messages_adapter_send_message,
                           g_steal_pointer (&error));
      return;
    }

  task = g_task_new (adapter, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_sms_device_send_message);
  g_task_set_task_data (task, g_object_ref (message), g_object_unref);

  valent_device_send_packet (self->device,
                             packet,
                             cancellable,
                             (GAsyncReadyCallback)valent_sms_device_send_message_cb,
                             g_object_ref (task));
}

/*
 * ValentObject
 */
static void
valent_sms_device_destroy (ValentObject *object)
{
  ValentSmsDevice *self = VALENT_SMS_DEVICE (object);

  g_clear_object (&self->device);
  g_clear_object (&self->connection);
  g_clear_object (&self->get_timestamp_stmt);

  VALENT_OBJECT_CLASS (valent_sms_device_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_sms_device_constructed (GObject *object)
{
  ValentSmsDevice *self = VALENT_SMS_DEVICE (object);

  G_OBJECT_CLASS (valent_sms_device_parent_class)->constructed (object);

  g_object_get (self, "connection", &self->connection, NULL);
}

static void
valent_sms_device_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentSmsDevice *self = VALENT_SMS_DEVICE (object);

  switch ((ValentSmsDeviceProperty)prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_device_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentSmsDevice *self = VALENT_SMS_DEVICE (object);

  switch ((ValentSmsDeviceProperty)prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_device_finalize (GObject *object)
{
  /* ValentSmsDevice *self = VALENT_SMS_DEVICE (object); */

  G_OBJECT_CLASS (valent_sms_device_parent_class)->finalize (object);
}

static void
valent_sms_device_class_init (ValentSmsDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentMessagesAdapterClass *adapter_class = VALENT_MESSAGES_ADAPTER_CLASS (klass);

  object_class->constructed = valent_sms_device_constructed;
  object_class->finalize = valent_sms_device_finalize;
  object_class->get_property = valent_sms_device_get_property;
  object_class->set_property = valent_sms_device_set_property;

  vobject_class->destroy = valent_sms_device_destroy;

  adapter_class->send_message = valent_sms_device_send_message;

  /**
   * ValentSmsDevice:device:
   *
   * The device hosting the message store.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                          VALENT_TYPE_DEVICE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_sms_device_init (ValentSmsDevice *self)
{
  g_queue_init (&self->attachment_requests);
  self->message_requests = g_ptr_array_new_with_free_func (g_free);
}

/**
 * valent_sms_device_new:
 * @device: a `ValentDevice`
 *
 * Create a new `ValentSmsDevice`.
 *
 * Returns: (transfer full): a new message store
 */
ValentMessagesAdapter *
valent_sms_device_new (ValentDevice *device)
{
  g_autoptr (ValentContext) context = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  context = valent_context_new (valent_device_get_context (device),
                                "plugin",
                                "sms");
  return g_object_new (VALENT_TYPE_SMS_DEVICE,
                       "object",  device,
                       "device",  device,
                       "context", context,
                       NULL);
}

static void
cursor_get_timestamp_cb (TrackerSparqlCursor *cursor,
                         GAsyncResult        *result,
                         gpointer             user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (GDateTime) datetime = NULL;
  g_autofree int64_t *timestamp = g_new0 (int64_t, 1);
  GError *error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error) &&
      tracker_sparql_cursor_is_bound (cursor, 0))
    {
      datetime = tracker_sparql_cursor_get_datetime (cursor, 0);
      *timestamp = g_date_time_to_unix_usec (datetime) / 1000;
    }
  tracker_sparql_cursor_close (cursor);

  if (error == NULL)
    g_task_return_pointer (task, g_steal_pointer (&timestamp), g_free);
  else
    g_task_return_error (task, g_steal_pointer (&error));

  g_free (timestamp);
}

static void
execute_get_timestamp_cb (TrackerSparqlStatement *stmt,
                          GAsyncResult           *result,
                          gpointer                user_data)
{
  g_autoptr (GTask) task = G_TASK (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GCancellable *cancellable = NULL;
  g_autoptr (GError) error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  cancellable = g_task_get_cancellable (G_TASK (result));
  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_get_timestamp_cb,
                                    g_object_ref (task));
}

static void
valent_sms_device_get_timestamp (ValentSmsDevice     *store,
                                 int64_t              thread_id,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;
  GError *error = NULL;

  g_return_if_fail (VALENT_IS_MESSAGES_ADAPTER (store));
  g_return_if_fail (thread_id >= 0);
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (store, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_sms_device_get_timestamp);

  if (store->get_timestamp_stmt == NULL)
    {
      store->get_timestamp_stmt =
        tracker_sparql_connection_load_statement_from_gresource (store->connection,
                                                                 GET_TIMESTAMP_RQ,
                                                                 cancellable,
                                                                 &error);
    }

  if (store->get_timestamp_stmt == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  tracker_sparql_statement_bind_int (store->get_timestamp_stmt,
                                     "threadId",
                                     thread_id);
  tracker_sparql_statement_execute_async (store->get_timestamp_stmt,
                                          cancellable,
                                          (GAsyncReadyCallback) execute_get_timestamp_cb,
                                          g_object_ref (task));
}

static int64_t
valent_sms_device_get_timestamp_finish (ValentSmsDevice  *store,
                                        GAsyncResult     *result,
                                        GError          **error)
{
  g_autofree int64_t *ret = NULL;

  g_return_val_if_fail (VALENT_IS_MESSAGES_ADAPTER (store), 0);
  g_return_val_if_fail (g_task_is_valid (result, store), 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  ret = g_task_propagate_pointer (G_TASK (result), error);

  return ret != NULL ? *ret : 0;
}

typedef struct
{
  ValentSmsDevice *self;
  int64_t thread_id;
  int64_t start_date;
  int64_t end_date;
  int64_t max_results;
} RequestData;

#define DEFAULT_MESSAGE_REQUEST (100)

static gboolean
find_message_request (gconstpointer a,
                      gconstpointer b)
{
  return ((RequestData *)a)->thread_id == *((int64_t *)b);
}

static void
find_message_range (JsonArray    *messages,
                    int64_t      *out_thread_id,
                    int64_t      *out_start_date,
                    int64_t      *out_end_date)
{
  unsigned int n_messages;

  g_assert (messages != NULL);
  g_assert (out_thread_id && out_start_date && out_end_date);

  *out_thread_id = 0;
  *out_start_date = INT64_MAX;
  *out_end_date = 0;

  n_messages = json_array_get_length (messages);
  for (unsigned int i = 0; i < n_messages; i++)
    {
      JsonObject *message = json_array_get_object_element (messages, i);
      int64_t date = json_object_get_int_member (message, "date");

      if (*out_thread_id == 0)
        *out_thread_id = json_object_get_int_member (message, "thread_id");

      if (*out_start_date > date)
        *out_start_date = date;

      if (*out_end_date < date)
        *out_end_date = date;
    }
}

static void
valent_sms_device_get_timestamp_cb (ValentSmsDevice *self,
                                    GAsyncResult    *result,
                                    gpointer         user_data)
{
  g_autofree RequestData *request = (RequestData *)user_data;
  int64_t cache_date;
  g_autoptr (GError) error = NULL;

  cache_date = valent_sms_device_get_timestamp_finish (self, result, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  if (cache_date < request->end_date)
    {
      request->start_date = cache_date;
      valent_sms_device_request_conversation (self,
                                              request->thread_id,
                                              request->end_date,
                                              request->max_results);
      g_ptr_array_add (self->message_requests, g_steal_pointer (&request));
    }
}

/**
 * valent_sms_device_handle_messages:
 * @self: a `ValentSmsDevice`
 * @packet: a `kdeconnect.sms.messages` packet
 *
 * Handle a packet of messages.
 */
void
valent_sms_device_handle_messages (ValentSmsDevice *self,
                                   JsonNode        *packet)
{
  ValentContext *context = NULL;
  g_autofree char *thread_iri = NULL;
  JsonNode *node;
  JsonObject *body;
  JsonArray *messages;
  unsigned int n_messages = 0;
  int64_t thread_id;
  int64_t start_date = 0, end_date = 0;
  unsigned int index_ = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_SMS_DEVICE (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);
  node = json_object_get_member (body, "messages");
  if (node == NULL || !JSON_NODE_HOLDS_ARRAY (node))
    {
      g_warning ("%s(): expected \"messages\" field holding an array", G_STRFUNC);
      return;
    }

  /* It's not clear if this could ever happen, or what it would imply if it did,
   * so log a debug message and bail.
   */
  messages = json_node_get_array (node);
  n_messages = json_array_get_length (messages);
  if (n_messages == 0)
    {
      g_debug ("%s(): expected \"messages\" field holding an array of objects",
               G_STRFUNC);
      return;
    }

  context = valent_extension_get_context (VALENT_EXTENSION (self));
  thread_id = json_object_get_int_member (json_array_get_object_element (messages, 0),
                                          "thread_id");
  thread_iri = g_strdup_printf ("valent://%s/%"PRId64,
                                valent_context_get_path (context),
                                thread_id);

  /* Check if there is an active request for this thread
   */
  find_message_range (messages, &thread_id, &start_date, &end_date);
  if (g_ptr_array_find_with_equal_func (self->message_requests,
                                        &thread_id,
                                        find_message_request,
                                        &index_))
    {
      RequestData *request = g_ptr_array_index (self->message_requests, index_);

      /* This is a response to our request
       */
      if (request->end_date == end_date)
        {
          if (n_messages >= request->max_results &&
              request->start_date < start_date)
            {
              request->end_date = start_date;
              valent_sms_device_request_conversation (self,
                                                      request->thread_id,
                                                      request->end_date,
                                                      request->max_results);
            }
          else
            {
              g_ptr_array_remove_index (self->message_requests, index_);
            }
        }
    }
  else if (n_messages == 1)
    {
      RequestData *request;

      request = g_new (RequestData, 1);
      request->thread_id = thread_id;
      request->start_date = end_date;
      request->end_date = end_date;
      request->max_results = DEFAULT_MESSAGE_REQUEST;

      valent_sms_device_get_timestamp (self,
                                       thread_id,
                                       self->cancellable,
                                       (GAsyncReadyCallback) valent_sms_device_get_timestamp_cb,
                                       g_steal_pointer (&request));
    }

  /* Store what we've received after the request is queued, otherwise having the
   * latest message we may request nothing.
   */
  valent_sms_device_add_json (self, node);

  VALENT_EXIT;
}

/**
 * valent_sms_device_handle_attachment_file:
 * @self: a `ValentSmsDevice`
 * @packet: a `kdeconnect.sms.attachment_file` packet
 *
 * Handle an attachment file.
 */
void
valent_sms_device_handle_attachment_file (ValentSmsDevice *self,
                                          JsonNode        *packet)
{
  ValentContext *context = NULL;
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autoptr (GFile) file = NULL;
  const char *filename = NULL;

  g_assert (VALENT_IS_SMS_DEVICE (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_string (packet, "filename", &filename))
    {
      g_warning ("%s(): expected \"filename\" field holding a string",
                 G_STRFUNC);
      return;
    }

  context = valent_extension_get_context (VALENT_EXTENSION (self));
  file = valent_context_get_cache_file (context, filename);
  transfer = valent_device_transfer_new (self->device, packet, file);
  valent_transfer_execute (transfer,
                           self->cancellable,
                           (GAsyncReadyCallback) handle_attachment_file_cb,
                           g_object_ref (self));
}

