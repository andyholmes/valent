// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-ui.h>

#include "valent-sms-message.h"
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

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentSmsPlugin, valent_sms_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

static ValentSmsMessage * valent_sms_plugin_deserialize_message (ValentSmsPlugin *self,
                                                                 JsonNode        *node);

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/*
 * Packet Helpers
 */
static gint64
get_message_date (JsonNode *message)
{
  JsonObject *body;

  body = json_node_get_object (message);

  return json_object_get_int_member (body, "date");
}

/**
 * sms_messages_get_thread_id
 * @messages: a #JsonArray
 *
 * Get the thread ID from the first message in @messages.
 *
 * Returns: a thread ID
 */
static gint64
sms_messages_get_thread_id (JsonArray *messages)
{
  JsonObject *message;

  message = json_array_get_object_element (messages, 0);

  return json_object_get_int_member (message, "thread_id");
}

/**
 * sms_messages_get_thread_ids
 * @messages: a #JsonArray
 *
 * Extract a list of thread ids from @messages, the body of a
 * `kdeconnect.sms.messages` packet
 *
 * Returns: (element-type gint64) (array zero-terminated=1): a list of ids
 */
static gint64 *
sms_messages_get_thread_ids (JsonArray *messages)
{
  JsonObject *message;
  guint i, n_messages;
  gint64 *ids;

  n_messages = json_array_get_length (messages);
  ids = g_new (gint64, n_messages + 1);

  for (i = 0; i < n_messages; i++)
    {
      message = json_array_get_object_element (messages, i);
      ids[i] = json_object_get_int_member (message, "thread_id");
    }
  ids[i] = 0;

  return ids;
}

/*
 * Packet Handlers
 */
static void
handle_conversation (ValentSmsPlugin *self,
                     JsonArray       *messages)
{
  guint i, n_messages;
  gint64 thread_id;
  gint64 cache_date;

  g_assert (VALENT_IS_SMS_PLUGIN (self));
  g_assert (messages != NULL);


  thread_id = sms_messages_get_thread_id (messages);
  cache_date = valent_sms_store_get_thread_date (self->store, thread_id);

  /* Handle each message */
  n_messages = json_array_get_length (messages);

  for (i = 0; i < n_messages; i++)
    {
      JsonNode *message_node;

      message_node = json_array_get_element (messages, i);

      if (cache_date < get_message_date (message_node))
        {
          g_autoptr (ValentSmsMessage) message = NULL;

          message = valent_sms_plugin_deserialize_message (self, message_node);
          valent_sms_store_add_message (self->store, message);
        }
    }

}

static void
handle_sms_messages (ValentSmsPlugin *self,
                     JsonNode        *packet)
{
  JsonObject *body;
  JsonArray *messages;
  guint i, n_messages;
  g_autofree gint64 *thread_ids = NULL;

  g_assert (VALENT_IS_SMS_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);
  messages = json_object_get_array_member (body, "messages");
  n_messages = json_array_get_length (messages);

  /* TODO: Since we don't know what this is a response to, it's not safe to
   *       assume it means _all_ messages should be deleted.
   */
  if (n_messages == 0)
    return;

  thread_ids = sms_messages_get_thread_ids (messages);

  /* If there's multiple thread_id's it's a summary of threads.
   */
  if (n_messages > 1 && thread_ids[0] != thread_ids[1])
    {
      /* TODO: Prune conversations */

      /* Request each new or newer thread */
      for (i = 0; i < n_messages; i++)
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
            valent_sms_plugin_request_conversation (self, thread_id);
        }
    }
  /* Otherwise this is single thread or new message */
  else
    {
      handle_conversation(self, messages);
    }
}

/**
 * Packet Providers
 */
void
valent_sms_plugin_request_conversation (ValentSmsPlugin *self,
                                        gint64           thread_id)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));
  g_return_if_fail (thread_id > 0);

  builder = valent_packet_start ("kdeconnect.sms.request_conversation");
  json_builder_set_member_name (builder, "threadID");
  json_builder_add_int_value (builder, thread_id);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

void
valent_sms_plugin_request_conversations (ValentSmsPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.sms.request_conversations");
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

void
valent_sms_plugin_request (ValentSmsPlugin *self,
                           const char      *address,
                           const char      *text)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_return_if_fail (VALENT_IS_SMS_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.sms.request");
  json_builder_set_member_name (builder, "sendSms");
  json_builder_add_boolean_value (builder, TRUE);
  json_builder_set_member_name (builder, "phoneNumber");
  json_builder_add_string_value (builder, address);
  json_builder_set_member_name (builder, "messageBody");
  json_builder_add_string_value (builder, text);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * GActions
 */
static gboolean
message_reply_cb (ValentSmsWindow  *window,
                  GList            *participants,
                  ValentSmsMessage *message,
                  ValentSmsPlugin  *self)
{
  const char *text;
  const char *address;

  if (g_list_length (participants) > 1)
    {
      g_warning ("Group messages not supported");
      return FALSE;
    }

  text = valent_sms_message_get_text (message);
  address = g_list_first (participants)->data;

  g_debug ("MESSAGE: %s", text);
  g_debug ("ADDRESS: %s", address);

  valent_sms_plugin_request (self, address, text);

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
                        G_CALLBACK (message_reply_cb),
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
static ValentSmsMessage *
valent_sms_plugin_deserialize_message (ValentSmsPlugin *self,
                                       JsonNode        *node)
{
  JsonObject *object, *sender_obj;
  JsonNode *addr_node;
  JsonArray *addr_array;
  GVariant *addresses;
  GVariantDict dict;

  ValentSmsMessageBox box;
  gint64 date;
  gint64 id;
  GVariant *metadata;
  gint64 read;
  const char *sender = NULL;
  const char *text = NULL;
  gint64 thread_id;
  gint64 event = 0;
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
      g_debug ("[%s] missing required field", G_STRFUNC);
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
  addr_array = json_node_get_array (addr_node);
  addresses = json_gvariant_deserialize (addr_node, "aa{sv}", NULL);

  /* Get the sender address, if incoming */
  if (box == VALENT_SMS_MESSAGE_BOX_INBOX)
    {
      sender_obj = json_array_get_object_element (addr_array, 0);
      sender = json_object_get_string_member (sender_obj, "address");
    }

  /* TODO: The `event` and `sub_id` fields are currently not implemented */
  if (json_object_has_member (object, "event"))
    event = json_object_get_int_member (object, "event");

  if (json_object_has_member (object, "sub_id"))
    sub_id = json_object_get_int_member (object, "sub_id");

  /* Build the metadata dictionary */
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert_value (&dict, "addresses", addresses);
  g_variant_dict_insert (&dict, "event", "i", event);
  g_variant_dict_insert (&dict, "sub_id", "i", sub_id);
  metadata = g_variant_dict_end (&dict);

  /* Build and return the message object */
  return g_object_new (VALENT_TYPE_SMS_MESSAGE,
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

  /* Unregister GMenu items */
  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));

  /* Unregister GActions */
  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));

  /* Close message window and drop SMS Store */
  g_clear_pointer (&self->window, gtk_window_destroy);
  g_clear_object (&self->store);
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

  /* GActions */
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
    handle_sms_messages (self, packet);
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

