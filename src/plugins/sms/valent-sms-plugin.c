// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-message.h"
#include "valent-sms-plugin.h"
#include "valent-sms-store.h"
#include "valent-sms-window.h"


struct _ValentSmsPlugin
{
  ValentDevicePlugin  parent_instance;

  ValentSmsStore    *store;
  GtkWindow         *window;
};

static ValentMessage * valent_sms_plugin_deserialize_message   (ValentSmsPlugin *self,
                                                                JsonNode        *node);
static void            valent_sms_plugin_request               (ValentSmsPlugin *self,
                                                                ValentMessage   *message);
static void            valent_sms_plugin_request_conversation  (ValentSmsPlugin *self,
                                                                int64_t          thread_id,
                                                                int64_t          start_date,
                                                                int64_t          max_results);
static void            valent_sms_plugin_request_conversations (ValentSmsPlugin *self);

G_DEFINE_FINAL_TYPE (ValentSmsPlugin, valent_sms_plugin, VALENT_TYPE_DEVICE_PLUGIN)


/**
 * message_hash:
 * @message_id: a message ID
 * @message_text: (nullable): a message text
 *
 * Shift the lower 32-bits of @message_id to the upper 32-bits of a 64-bit
 * integer, then set the lower 32-bits to a djb2 hash of @message_text.
 *
 * This hack is necessary because kdeconnect-android pulls SMS and MMS from
 * separate tables so two messages (even in the same thread) may share an ID.
 * The timestamp would be an ideal alternative except that it can change,
 * possibly when moved between boxes (eg. outbox => sent).
 *
 * Returns: a unique ID
 */
static inline int64_t
message_hash (int64_t     message_id,
              const char *message_text)
{
  uint32_t hash = 5381;

  if G_UNLIKELY (message_text == NULL)
    message_text = "";

  // djb2
  for (unsigned int i = 0; message_text[i]; i++)
    hash = ((hash << 5L) + hash) + message_text[i]; /* hash * 33 + c */

  return (((uint64_t) message_id) << 32) | hash;
}

static ValentMessage *
valent_sms_plugin_deserialize_message (ValentSmsPlugin *self,
                                       JsonNode        *node)
{
  JsonObject *object;
  JsonNode *addr_node;
  GVariant *addresses;
  GVariantDict dict;

  ValentMessageBox box;
  int64_t date;
  int64_t id;
  GVariant *metadata;
  int64_t read;
  const char *sender = NULL;
  const char *text = NULL;
  int64_t thread_id;
  ValentMessageFlags event = VALENT_MESSAGE_FLAGS_UNKNOWN;
  int64_t sub_id = -1;

  g_assert (VALENT_IS_SMS_PLUGIN (self));
  g_assert (JSON_NODE_HOLDS_OBJECT (node));

  object = json_node_get_object (node);

  /* Check all the required fields exist */
  if G_UNLIKELY (!json_object_has_member (object, "thread_id") ||
                 !json_object_has_member (object, "_id") ||
                 !json_object_has_member (object, "body") ||
                 !json_object_has_member (object, "date") ||
                 !json_object_has_member (object, "read") ||
                 !json_object_has_member (object, "type") ||
                 !json_object_has_member (object, "addresses"))
    {
      g_warning ("%s(): missing required message field", G_STRFUNC);
      return NULL;
    }

  /* Basic fields */
  box = json_object_get_int_member (object, "type");
  date = json_object_get_int_member (object, "date");
  id = json_object_get_int_member (object, "_id");
  read = json_object_get_int_member (object, "read");
  text = json_object_get_string_member (object, "body");
  thread_id = json_object_get_int_member (object, "thread_id");

  /* Addresses */
  addr_node = json_object_get_member (object, "addresses");
  addresses = json_gvariant_deserialize (addr_node, "aa{sv}", NULL);

  /* If incoming, the first address will be the sender */
  if (box == VALENT_MESSAGE_BOX_INBOX)
    {
      JsonObject *sender_obj;
      JsonArray *addr_array;

      addr_array = json_node_get_array (addr_node);

      if (json_array_get_length (addr_array) > 0)
        {
          sender_obj = json_array_get_object_element (addr_array, 0);
          sender = json_object_get_string_member (sender_obj, "address");
        }
      else
        g_warning ("No address for message %"G_GINT64_FORMAT" in thread %"G_GINT64_FORMAT, id, thread_id);
    }

  /* TODO: The `event` and `sub_id` fields are currently not implemented */
  if (json_object_has_member (object, "event"))
    event = json_object_get_int_member (object, "event");

  if (json_object_has_member (object, "sub_id"))
    sub_id = json_object_get_int_member (object, "sub_id");

  /* HACK: try to create a truly unique ID from a potentially non-unique ID */
  id = message_hash (id, text);

  /* Build the metadata dictionary */
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert_value (&dict, "addresses", addresses);
  g_variant_dict_insert (&dict, "event", "u", event);
  g_variant_dict_insert (&dict, "sub_id", "i", sub_id);
  metadata = g_variant_dict_end (&dict);

  /* Build and return the message object */
  return g_object_new (VALENT_TYPE_MESSAGE,
                       "box",       box,
                       "date",      date,
                       "id",        id,
                       "metadata",  metadata,
                       "read",      read,
                       "sender",    sender,
                       "text",      text,
                       "thread-id", thread_id,
                       NULL);
}

static void
valent_sms_plugin_handle_thread (ValentSmsPlugin *self,
                                 JsonArray       *messages)
{
  g_autoptr (GPtrArray) results = NULL;
  unsigned int n_messages;

  g_assert (VALENT_IS_SMS_PLUGIN (self));
  g_assert (messages != NULL);

  /* Handle each message */
  n_messages = json_array_get_length (messages);
  results = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < n_messages; i++)
    {
      JsonNode *message_node;
      ValentMessage *message;

      message_node = json_array_get_element (messages, i);
      message = valent_sms_plugin_deserialize_message (self, message_node);
      g_ptr_array_add (results, message);
    }

  valent_sms_store_add_messages (self->store, results, NULL, NULL, NULL);
}

static void
valent_sms_plugin_handle_messages (ValentSmsPlugin *self,
                                   JsonNode        *packet)
{
  JsonObject *body;
  JsonArray *messages;
  unsigned int n_messages;

  g_assert (VALENT_IS_SMS_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);
  messages = json_object_get_array_member (body, "messages");
  n_messages = json_array_get_length (messages);

  /* It's not clear if this could ever happen, or what it would imply if it did,
   * so log a debug message and bail.
   */
  if (n_messages == 0)
    {
      g_debug ("%s(): expected \"messages\" field holding an array of objects",
               G_STRFUNC);
      return;
    }

  /* If there is more than one message then this is either a response to a
   * request for a thread, or an old client sending a summary of threads. If we
   * assume the latter it may cause a multi-device infinite-loop.
   */
  if (n_messages == 1)
    {
      JsonObject *message;
      int64_t thread_id;
      int64_t thread_date;
      int64_t cache_date;

      // TODO: handle missing or invalid fields
      message = json_array_get_object_element (messages, 0);
      thread_id = json_object_get_int_member (message, "thread_id");
      thread_date = json_object_get_int_member (message, "date");

      /* Get the last cached date and compare timestamps */
      cache_date = valent_sms_store_get_thread_date (self->store, thread_id);

      if (cache_date < thread_date)
        valent_sms_plugin_request_conversation (self, thread_id, cache_date, 0);
    }

  /* Store what we've received after the request is queued, otherwise having the
   * latest message we may request nothing.
   */
  valent_sms_plugin_handle_thread (self, messages);
}

static void
valent_sms_plugin_request_conversation (ValentSmsPlugin *self,
                                        int64_t          thread_id,
                                        int64_t          start_date,
                                        int64_t          max_results)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));
  g_return_if_fail (thread_id >= 0);

  valent_packet_init (&builder, "kdeconnect.sms.request_conversation");
  json_builder_set_member_name (builder, "threadID");
  json_builder_add_int_value (builder, thread_id);

  if (start_date > 0)
    {
      json_builder_set_member_name (builder, "rangeStartTimestamp");
      json_builder_add_int_value (builder, start_date);
    }

  if (max_results > 0)
    {
      json_builder_set_member_name (builder, "numberToRequest");
      json_builder_add_int_value (builder, max_results);
    }

  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_sms_plugin_request_conversations (ValentSmsPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.sms.request_conversations");
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_sms_plugin_request (ValentSmsPlugin *self,
                           ValentMessage   *message)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  GVariant *metadata;
  g_autoptr (GVariant) addresses = NULL;
  JsonNode *addresses_node = NULL;
  int sub_id = -1;
  const char *text;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  // Get the data
  if ((metadata = valent_message_get_metadata (message)) == NULL)
    g_return_if_reached ();

  if ((addresses = g_variant_lookup_value (metadata, "addresses", NULL)) == NULL)
    g_return_if_reached ();

  if (!g_variant_lookup (metadata, "sub_id", "i", &sub_id))
      sub_id = -1;

  // Build the packet
  valent_packet_init (&builder, "kdeconnect.sms.request");

  json_builder_set_member_name (builder, "version");
  json_builder_add_int_value (builder, 2);

  addresses_node = json_gvariant_serialize (addresses);
  json_builder_set_member_name (builder, "addresses");
  json_builder_add_value (builder, addresses_node);

  text = valent_message_get_text (message);
  json_builder_set_member_name (builder, "messageBody");
  json_builder_add_string_value (builder, text);

  json_builder_set_member_name (builder, "subID");
  json_builder_add_int_value (builder, sub_id);

  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * GActions
 */
static void
fetch_action (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (user_data);

  g_assert (VALENT_IS_SMS_PLUGIN (self));

  valent_sms_plugin_request_conversations (self);
}

static gboolean
on_send_message (ValentSmsWindow *window,
                 ValentMessage   *message,
                 ValentSmsPlugin *self)
{
  g_assert (VALENT_IS_SMS_WINDOW (window));
  g_assert (VALENT_IS_MESSAGE (message));

  valent_sms_plugin_request (self, message);

  return TRUE;
}

static void
messaging_action (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (user_data);
  ValentDevice *device;

  g_assert (VALENT_IS_SMS_PLUGIN (self));

  if (!gtk_is_initialized ())
    {
      g_warning ("%s: No display available", G_STRFUNC);
      return;
    }

  if (self->window == NULL)
    {
      ValentContactStore *store;

      device = valent_extension_get_object (VALENT_EXTENSION (self));
      store = valent_contacts_ensure_store (valent_contacts_get_default (),
                                            valent_device_get_id (device),
                                            valent_device_get_name (device));

      self->window = g_object_new (VALENT_TYPE_SMS_WINDOW,
                                   "contact-store", store,
                                   "message-store", self->store,
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (self->window),
                                 (gpointer) &self->window);

      g_signal_connect_object (self->window,
                               "send-message",
                               G_CALLBACK (on_send_message),
                               self, 0);
    }

  gtk_window_present_with_time (GTK_WINDOW (self->window), GDK_CURRENT_TIME);
}

static const GActionEntry actions[] = {
    {"fetch",     fetch_action,     NULL, NULL, NULL},
    {"messaging", messaging_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_sms_plugin_update_state (ValentDevicePlugin *plugin,
                                ValentDeviceState   state)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_SMS_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);

  /* Request summary of messages */
  if (available)
    valent_sms_plugin_request_conversations (self);
}

static void
valent_sms_plugin_handle_packet (ValentDevicePlugin *plugin,
                                 const char         *type,
                                 JsonNode           *packet)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (plugin);

  g_assert (VALENT_IS_SMS_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_str_equal (type, "kdeconnect.sms.messages"))
    valent_sms_plugin_handle_messages (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_sms_plugin_destroy (ValentObject *object)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  /* Close message window and drop SMS Store */
  g_clear_pointer (&self->window, gtk_window_destroy);
  g_clear_object (&self->store);

  valent_device_plugin_set_menu_item (plugin, "device.sms.messaging", NULL);

  VALENT_OBJECT_CLASS (valent_sms_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_sms_plugin_constructed (GObject *object)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (object);
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);
  ValentDevice *device;
  ValentContext *context = NULL;

  /* Load SMS Store */
  device = valent_extension_get_object (VALENT_EXTENSION (self));
  context = valent_device_get_context (device);
  self->store = g_object_new (VALENT_TYPE_SMS_STORE,
                              "domain", "plugin",
                              "id",     "sms",
                              "parent", context,
                              NULL);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_set_menu_action (plugin,
                                        "device.sms.messaging",
                                        _("Messaging"),
                                        "sms-symbolic");

  G_OBJECT_CLASS (valent_sms_plugin_parent_class)->constructed (object);
}

static void
valent_sms_plugin_finalize (GObject *object)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (object);

  if (self->window)
    g_clear_pointer (&self->window, gtk_window_destroy);
  g_clear_object (&self->store);

  G_OBJECT_CLASS (valent_sms_plugin_parent_class)->finalize (object);
}

static void
valent_sms_plugin_class_init (ValentSmsPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->finalize = valent_sms_plugin_finalize;

  object_class->constructed = valent_sms_plugin_constructed;
  plugin_class->handle_packet = valent_sms_plugin_handle_packet;
  plugin_class->update_state = valent_sms_plugin_update_state;

  vobject_class->destroy = valent_sms_plugin_destroy;
}

static void
valent_sms_plugin_init (ValentSmsPlugin *self)
{
}

