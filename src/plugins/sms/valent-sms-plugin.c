// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-message.h"
#include "valent-sms-plugin.h"
#include "valent-sms-store.h"
#include "valent-sms-window.h"


struct _ValentSmsPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  ValentSmsStore    *store;
  GtkWindow         *window;
};

static ValentMessage * valent_sms_plugin_deserialize_message   (ValentSmsPlugin             *self,
                                                                JsonNode                    *node);
static void            valent_sms_plugin_request               (ValentSmsPlugin             *self,
                                                                ValentMessage               *message);
static void            valent_sms_plugin_request_conversation  (ValentSmsPlugin             *self,
                                                                gint64                       thread_id,
                                                                gint64                       start_date,
                                                                gint64                       max_results);
static void            valent_sms_plugin_request_conversations (ValentSmsPlugin             *self);

static void            valent_device_plugin_iface_init         (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentSmsPlugin, valent_sms_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


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
static inline gint64
message_hash (gint64      message_id,
              const char *message_text)
{
  guint32 hash = 5381;

  if G_UNLIKELY (message_text == NULL)
    message_text = "";

  // djb2
  for (unsigned int i = 0; message_text[i]; i++)
    hash = ((hash << 5L) + hash) + message_text[i]; /* hash * 33 + c */

  return (((guint64) message_id) << 32) | hash;
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
  gint64 date;
  gint64 id;
  GVariant *metadata;
  gint64 read;
  const char *sender = NULL;
  const char *text = NULL;
  gint64 thread_id;
  ValentMessageFlags event = VALENT_MESSAGE_FLAGS_UNKNOWN;
  gint64 sub_id = -1;

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

/**
 * messages_is_thread:
 * @messages: a #JsonArray
 *
 * Check if @messages is a thread of messages, or a summary of threads.
 *
 * Returns: %TRUE if @messages is a conversation thread
 */
static gboolean
messages_is_thread (JsonArray *messages)
{
  JsonObject *message;
  gint64 first, second;

  /* TODO: A thread with a single message can't be distinguished from
   *       a summary with a single thread; in fact both could be true.
   *       If we assume the latter is true exclusively, we will get
   *       caught in a loop requesting the full thread. */
  if (json_array_get_length (messages) < 2)
    return TRUE;

  message = json_array_get_object_element (messages, 0);
  first = json_object_get_int_member (message, "thread_id");

  message = json_array_get_object_element (messages, 1);
  second = json_object_get_int_member (message, "thread_id");

  return first == second;
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

  /* This would typically mean "all threads have been deleted", but it's more
   * reasonable to assume this was the result of an error. */
  if (n_messages == 0)
    return;

  /* If this is a thread of messages we'll add them to the store */
  if (messages_is_thread (messages))
    {
      valent_sms_plugin_handle_thread (self, messages);
      return;
    }

  /* If this is a summary of threads we'll request each new thread */
  for (unsigned int i = 0; i < n_messages; i++)
    {
      JsonObject *message;
      gint64 thread_id;
      gint64 thread_date;
      gint64 cache_date;

      message = json_array_get_object_element (messages, i);
      thread_id = json_object_get_int_member (message, "thread_id");
      thread_date = json_object_get_int_member (message, "date");

      /* Get the last cached date and compare timestamps */
      cache_date = valent_sms_store_get_thread_date (self->store, thread_id);

      if (cache_date < thread_date)
        valent_sms_plugin_request_conversation (self, thread_id, cache_date, 0);
    }
}

static void
valent_sms_plugin_request_conversation (ValentSmsPlugin *self,
                                        gint64           thread_id,
                                        gint64           start_date,
                                        gint64           max_results)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));
  g_return_if_fail (thread_id >= 0);

  builder = valent_packet_start ("kdeconnect.sms.request_conversation");
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

  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_sms_plugin_request_conversations (ValentSmsPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.sms.request_conversations");
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_sms_plugin_request (ValentSmsPlugin *self,
                           ValentMessage   *message)
{
  JsonBuilder *builder;
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
  builder = valent_packet_start ("kdeconnect.sms.request");

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

  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * GActions
 */
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

  g_assert (VALENT_IS_SMS_PLUGIN (self));

  if (self->window == NULL)
    {
      ValentContactStore *store;

      store = valent_contacts_ensure_store (valent_contacts_get_default (),
                                            valent_device_get_id (self->device),
                                            valent_device_get_name (self->device));

      self->window = g_object_new (VALENT_TYPE_SMS_WINDOW,
                                   "application",   g_application_get_default (),
                                   "contact-store", store,
                                   "message-store", self->store,
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (self->window),
                                 (gpointer) &self->window);

      g_signal_connect (self->window,
                        "send-message",
                        G_CALLBACK (on_send_message),
                        self);
    }

  gtk_window_present_with_time (GTK_WINDOW (self->window), GDK_CURRENT_TIME);
}

static void
sms_fetch_action (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (user_data);

  g_assert (VALENT_IS_SMS_PLUGIN (self));

  valent_sms_plugin_request_conversations (self);
}

static const GActionEntry actions[] = {
    {"messaging", messaging_action, NULL, NULL, NULL},
    {"sms-fetch", sms_fetch_action, NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Messaging"), "device.messaging", "sms-symbolic"}
};

/*
 * Private Methods
 */


/*
 * ValentDevicePlugin
 */
static void
valent_sms_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (plugin);
  g_autoptr (ValentData) data = NULL;

  g_assert (VALENT_IS_SMS_PLUGIN (plugin));

  /* Load SMS Store */
  data = valent_device_ref_data (self->device);
  self->store = g_object_new (VALENT_TYPE_SMS_STORE,
                              "context", "sms",
                              "parent",  data,
                              NULL);

  /* Register GActions */
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));

  /* Register GMenu items */
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_sms_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (plugin);

  g_assert (VALENT_IS_SMS_PLUGIN (plugin));

  /* Close message window and drop SMS Store */
  g_clear_pointer (&self->window, gtk_window_destroy);
  g_clear_object (&self->store);

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));
}

static void
valent_sms_plugin_update_state (ValentDevicePlugin *plugin,
                                ValentDeviceState   state)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_SMS_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_device_plugin_toggle_actions (plugin,
                                       actions, G_N_ELEMENTS (actions),
                                       available);

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

  if (g_strcmp0 (type, "kdeconnect.sms.messages") == 0)
    valent_sms_plugin_handle_messages (self, packet);
  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_sms_plugin_enable;
  iface->disable = valent_sms_plugin_disable;
  iface->handle_packet = valent_sms_plugin_handle_packet;
  iface->update_state = valent_sms_plugin_update_state;
}

/*
 * GObject
 */
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
valent_sms_plugin_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_plugin_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_plugin_class_init (ValentSmsPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_autoptr (GtkCssProvider) theme = NULL;

  object_class->finalize = valent_sms_plugin_finalize;
  object_class->get_property = valent_sms_plugin_get_property;
  object_class->set_property = valent_sms_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");

  theme = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (theme, "/plugins/sms/sms.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (theme),
                                              GTK_STYLE_PROVIDER_PRIORITY_USER);
}

static void
valent_sms_plugin_init (ValentSmsPlugin *self)
{
}

