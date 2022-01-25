// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-telephony-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>
#include <libvalent-ui.h>

#include "valent-telephony-plugin.h"


struct _ValentTelephonyPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  GSettings         *settings;

  gpointer           prev_input;
  gpointer           prev_output;
  gboolean           prev_paused;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentTelephonyPlugin, valent_telephony_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};


/*
 * StreamState Helpers
 */
typedef struct
{
  GWeakRef      stream;

  unsigned int  current_level;
  unsigned int  current_muted : 1;
  unsigned int  original_level;
  unsigned int  original_muted : 1;
} StreamState;

static StreamState *
stream_state_new (ValentMixerStream *stream,
                  int                level)
{
  StreamState *state;

  state = g_new0 (StreamState, 1);
  g_weak_ref_init (&state->stream, stream);
  state->original_level = valent_mixer_stream_get_level (stream);
  state->original_muted = valent_mixer_stream_get_muted (stream);

  if (level == 0)
    {
      state->current_level = valent_mixer_stream_get_level (stream);
      state->current_muted = TRUE;

      valent_mixer_stream_set_muted (stream, TRUE);
    }
  else if (level > 0)
    {
      state->current_level = level;
      state->current_muted = valent_mixer_stream_get_muted (stream);

      valent_mixer_stream_set_level (stream, level);
    }

  return state;
}

static void
stream_state_update (StreamState       *state,
                     ValentMixerStream *stream,
                     int                level)
{
  g_autoptr (ValentMixerStream) current_stream = NULL;

  /* If the active stream has changed, bail instead of guessing what to do */
  if ((current_stream = g_weak_ref_get (&state->stream)) != stream)
    {
      g_weak_ref_set (&state->stream, NULL);
      return;
    }

  if (level == 0)
    {
      state->current_muted = TRUE;
      valent_mixer_stream_set_muted (stream, TRUE);
    }
  else if (level > 0)
    {
      state->current_level = level;
      valent_mixer_stream_set_level (stream, level);
    }
}

static inline void
stream_state_restore (gpointer data)
{
  StreamState *state = data;
  g_autoptr (ValentMixerStream) stream = NULL;

  if ((stream = g_weak_ref_get (&state->stream)) != NULL)
    {
      if (valent_mixer_stream_get_level (stream) == state->current_level)
        valent_mixer_stream_set_level (stream, state->original_level);

      if (valent_mixer_stream_get_muted (stream) == state->current_muted)
        valent_mixer_stream_set_muted (stream, state->original_muted);
    }

  g_weak_ref_clear (&state->stream);
  g_clear_pointer (&state, g_free);
}

static inline void
stream_state_free (gpointer data)
{
  StreamState *state = data;

  g_weak_ref_clear (&state->stream);
  g_clear_pointer (&state, g_free);
}

static void
valent_telephony_plugin_restore_media_state (ValentTelephonyPlugin *self)
{
  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  g_clear_pointer (&self->prev_output, stream_state_restore);
  g_clear_pointer (&self->prev_input, stream_state_restore);

  if (self->prev_paused)
    {
      ValentMedia *media;

      media = valent_media_get_default ();
      valent_media_unpause (media);
      self->prev_paused = FALSE;
    }
}

static void
valent_telephony_plugin_update_media_state (ValentTelephonyPlugin *self,
                                            const char            *event)
{
  ValentMixer *mixer;
  ValentMixerStream *stream;
  int output_level;
  int input_level;
  gboolean pause = FALSE;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));
  g_assert (event != NULL && *event != '\0');

  /* Retrieve the user preference for this event */
  if (g_strcmp0 (event, "ringing") == 0)
    {
      output_level = g_settings_get_int (self->settings, "ringing-volume");
      input_level = g_settings_get_int (self->settings, "ringing-microphone");
      pause = g_settings_get_boolean (self->settings, "ringing-pause");
    }
  else if (g_strcmp0 (event, "talking") == 0)
    {
      output_level = g_settings_get_int (self->settings, "talking-volume");
      input_level = g_settings_get_int (self->settings, "talking-microphone");
      pause = g_settings_get_boolean (self->settings, "talking-pause");
    }
  else
    g_assert_not_reached ();

  /* Speakers & Microphone */
  mixer = valent_mixer_get_default ();

  if ((stream = valent_mixer_get_default_output (mixer)) != NULL)
    {
      if (self->prev_output == NULL)
        self->prev_output = stream_state_new (stream, output_level);
      else
        stream_state_update (self->prev_output, stream, output_level);
    }

  if ((stream = valent_mixer_get_default_input (mixer)) != NULL)
    {
      if (self->prev_input == NULL)
        self->prev_input = stream_state_new (stream, input_level);
      else
        stream_state_update (self->prev_input, stream, input_level);
    }

  /* Media Players */
  if (pause)
    {
      ValentMedia *media;

      media = valent_media_get_default ();
      valent_media_pause (media);
      self->prev_paused = TRUE;
    }
}

/**
 * <private>
 * get_event_icon:
 * @body: a #JsonObject body from a telephony packet
 *
 * Return a #GIcon for a telephony notification, created from a base64 encoded
 * avatar if available.
 *
 * Returns: (transfer full): a new #GIcon
 */
static GIcon *
get_event_icon (JsonObject *body)
{
  const char *data;

  g_assert (body != NULL);

  if ((data = valent_packet_check_string (body, "phoneThumbnail")) != NULL)
    {
      GdkPixbuf *pixbuf;

      if ((pixbuf = valent_ui_pixbuf_from_base64 (data, NULL)) != NULL)
        return G_ICON (g_object_ref (pixbuf));
    }

  return g_themed_icon_new ("call-start-symbolic");
}

static void
valent_telephony_plugin_handle_telephony (ValentTelephonyPlugin *self,
                                          JsonNode              *packet)
{
  JsonObject *body;
  const char *event;
  const char *sender;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;
  g_autofree char *id = NULL;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);
  event = json_object_get_string_member (body, "event");

  /* Currently, only "ringing" and "talking" events are supported */
  if (g_strcmp0 (event, "ringing") != 0 && g_strcmp0 (event, "talking") != 0)
    {
      VALENT_TODO ("\"%s\" event", event);
      return;
    }

  /* Sender*/
  if (json_object_has_member (body, "contactName"))
    sender = json_object_get_string_member (body, "contactName");
  else if (json_object_has_member (body, "phoneNumber"))
    sender = json_object_get_string_member (body, "phoneNumber");
  else
    sender = _("Unknown Contact");

  /* Inject the event type into the notification ID, to distinguish them */
  id = g_strdup_printf ("%s|%s", event, sender);

  /* This is a cancelled event */
  if (valent_packet_check_boolean (body, "isCancel"))
    {
      valent_telephony_plugin_restore_media_state (self);
      valent_device_hide_notification (self->device, id);
      return;
    }

  /* Adjust volume/pause media */
  valent_telephony_plugin_update_media_state (self, event);

  /* Notify user */
  notification = g_notification_new (sender);
  icon = get_event_icon (body);
  g_notification_set_icon (notification, icon);

  if (g_strcmp0 (event, "ringing") == 0)
    {
      g_notification_set_body (notification, _("Incoming call"));
      valent_notification_add_device_button (notification,
                                             self->device,
                                             _("Mute"),
                                             "mute-call",
                                             NULL);
      g_notification_set_priority (notification,
                                   G_NOTIFICATION_PRIORITY_URGENT);
    }
  else if (g_strcmp0 (event, "talking") == 0)
    {
      g_notification_set_body (notification, _("Ongoing call"));
    }

  valent_device_show_notification (self->device, id, notification);
}

static void
valent_telephony_plugin_mute_call (ValentTelephonyPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.telephony.request_mute");
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

/*
 * GActions
 */
static void
mute_action (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (user_data);

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  valent_telephony_plugin_mute_call (self);
}

static const GActionEntry actions[] = {
    {"mute-call", mute_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
static void
valent_telephony_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);
  const char *device_id;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  device_id = valent_device_get_id (self->device);
  self->settings = valent_device_plugin_new_settings (device_id, "telephony");
  valent_device_plugin_register_actions (plugin,
                                         actions,
                                         G_N_ELEMENTS (actions));
}

static void
valent_telephony_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);

  /* We're about to be disposed, so clear the stored states */
  g_clear_pointer (&self->prev_output, stream_state_free);
  g_clear_pointer (&self->prev_input, stream_state_free);

  valent_device_plugin_unregister_actions (plugin,
                                           actions,
                                           G_N_ELEMENTS (actions));
  g_clear_object (&self->settings);
}

static void
valent_telephony_plugin_update_state (ValentDevicePlugin *plugin,
                                      ValentDeviceState   state)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (plugin));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  /* Clear the stream state, but don't restore it as there may still be an
   * event in progress. */
  if (!available)
    {
      g_clear_pointer (&self->prev_output, stream_state_free);
      g_clear_pointer (&self->prev_input, stream_state_free);
    }

  valent_device_plugin_toggle_actions (plugin,
                                       actions,
                                       G_N_ELEMENTS (actions),
                                       available);
}

static void
valent_telephony_plugin_handle_packet (ValentDevicePlugin *plugin,
                                       const char         *type,
                                       JsonNode           *packet)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (plugin);

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_strcmp0 (type, "kdeconnect.telephony") == 0)
    valent_telephony_plugin_handle_telephony (self, packet);
  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_telephony_plugin_enable;
  iface->disable = valent_telephony_plugin_disable;
  iface->handle_packet = valent_telephony_plugin_handle_packet;
  iface->update_state = valent_telephony_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_telephony_plugin_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (object);

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
valent_telephony_plugin_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (object);

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
valent_telephony_plugin_class_init (ValentTelephonyPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_telephony_plugin_get_property;
  object_class->set_property = valent_telephony_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_telephony_plugin_init (ValentTelephonyPlugin *self)
{
}

