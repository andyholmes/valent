// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-sms-device.h"

#include "valent-sms-plugin.h"


struct _ValentSmsPlugin
{
  ValentDevicePlugin     parent_instance;

  ValentMessagesAdapter *store;
  GCancellable          *cancellable;
};

G_DEFINE_FINAL_TYPE (ValentSmsPlugin, valent_sms_plugin, VALENT_TYPE_DEVICE_PLUGIN)

/*
 * GActions
 */
static void
sync_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (user_data);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_SMS_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.sms.request_conversations");
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static const GActionEntry actions[] = {
    {"sync", sync_action, NULL, NULL, NULL},
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
    {
      if (self->cancellable == NULL)
        {
          self->cancellable = g_cancellable_new ();
          g_action_group_activate_action (G_ACTION_GROUP (self), "sync", NULL);
        }
    }
  else
    {
      if (self->cancellable != NULL)
        {
          g_cancellable_cancel (self->cancellable);
          g_clear_object (&self->cancellable);
        }
    }
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
    valent_sms_device_handle_messages (VALENT_SMS_DEVICE (self->store), packet);
  else if (g_str_equal (type, "kdeconnect.sms.attachment_file"))
    valent_sms_device_handle_attachment_file (VALENT_SMS_DEVICE (self->store), packet);
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

  if (self->store != NULL)
    {
      valent_messages_unexport_adapter (valent_messages_get_default (),
                                        self->store);
      g_clear_object (&self->store);
    }

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
  ValentDevice *device = NULL;

  G_OBJECT_CLASS (valent_sms_plugin_parent_class)->constructed (object);

  device = valent_extension_get_object (VALENT_EXTENSION (plugin));
  self->store = valent_sms_device_new (device);
  valent_messages_export_adapter (valent_messages_get_default (), self->store);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
}

static void
valent_sms_plugin_finalize (GObject *object)
{
  ValentSmsPlugin *self = VALENT_SMS_PLUGIN (object);

  g_clear_object (&self->store);

  G_OBJECT_CLASS (valent_sms_plugin_parent_class)->finalize (object);
}

static void
valent_sms_plugin_class_init (ValentSmsPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_sms_plugin_constructed;
  object_class->finalize = valent_sms_plugin_finalize;

  plugin_class->handle_packet = valent_sms_plugin_handle_packet;
  plugin_class->update_state = valent_sms_plugin_update_state;

  vobject_class->destroy = valent_sms_plugin_destroy;
}

static void
valent_sms_plugin_init (ValentSmsPlugin *self)
{
}

