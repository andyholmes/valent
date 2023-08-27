// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-telephony-plugin"

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-telephony-plugin.h"


struct _ValentTelephonyPlugin
{
  ValentDevicePlugin  parent_instance;

  gpointer           prev_input;
  gpointer           prev_output;
  gboolean           prev_paused;
};

G_DEFINE_FINAL_TYPE (ValentTelephonyPlugin, valent_telephony_plugin, VALENT_TYPE_DEVICE_PLUGIN)


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
  GSettings *settings;
  ValentMixer *mixer;
  ValentMixerStream *stream;
  int output_level;
  int input_level;
  gboolean pause = FALSE;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));
  g_assert (event != NULL && *event != '\0');

  settings = valent_extension_get_settings (VALENT_EXTENSION (self));

  /* Retrieve the user preference for this event */
  if (g_str_equal (event, "ringing"))
    {
      output_level = g_settings_get_int (settings, "ringing-volume");
      input_level = g_settings_get_int (settings, "ringing-microphone");
      pause = g_settings_get_boolean (settings, "ringing-pause");
    }
  else if (g_str_equal (event, "talking"))
    {
      output_level = g_settings_get_int (settings, "talking-volume");
      input_level = g_settings_get_int (settings, "talking-microphone");
      pause = g_settings_get_boolean (settings, "talking-pause");
    }
  else
    {
      g_return_if_reached ();
    }

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

static GIcon *
valent_telephony_plugin_get_event_icon (JsonNode   *packet,
                                        const char *event)
{
  const char *phone_thumbnail = NULL;

  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_get_string (packet, "phoneThumbnail", &phone_thumbnail))
    {
      g_autoptr (GdkPixbufLoader) loader = NULL;
      GdkPixbuf *pixbuf = NULL;
      g_autoptr (GError) error = NULL;
      g_autofree unsigned char *data = NULL;
      size_t dlen;

      data = g_base64_decode (phone_thumbnail, &dlen);
      loader = gdk_pixbuf_loader_new();

      if (gdk_pixbuf_loader_write (loader, data, dlen, &error) &&
          gdk_pixbuf_loader_close (loader, &error))
        pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

      if (error != NULL)
        g_debug ("%s(): %s", G_STRFUNC, error->message);

      if (pixbuf != NULL)
        return G_ICON (g_object_ref (pixbuf));
    }

  if (g_str_equal (event, "ringing"))
    return g_themed_icon_new ("call-incoming-symbolic");

  if (g_str_equal (event, "talking"))
    return g_themed_icon_new ("call-start-symbolic");

  if (g_str_equal (event, "missedCall"))
    return g_themed_icon_new ("call-missed-symbolic");

  return NULL;
}

static void
valent_telephony_plugin_handle_telephony (ValentTelephonyPlugin *self,
                                          JsonNode              *packet)
{
  ValentDevice *device;
  const char *event;
  const char *sender;
  g_autoptr (GNotification) notification = NULL;
  g_autoptr (GIcon) icon = NULL;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (!valent_packet_get_string (packet, "event", &event))
    {
      g_debug ("%s(): expected \"event\" field holding a string",
               G_STRFUNC);
      return;
    }

  /* Currently, only "ringing" and "talking" events are supported */
  if (!g_str_equal (event, "ringing") && !g_str_equal (event, "talking"))
    {
      VALENT_NOTE ("TODO: \"%s\" event", event);
      return;
    }

  /* Sender*/
  if (!valent_packet_get_string (packet, "contactName", &sender) &&
      !valent_packet_get_string (packet, "phoneNumber", &sender))
    sender = _("Unknown Contact");

  /* The sender is injected into the notification ID, since it's possible an
   * event could occur for multiple callers concurrently.
   *
   * Because we only support voice events, we can be certain that subsequent
   * events from the same sender supersede previous events, and replace the
   * older notifications.
   */
  device = valent_extension_get_object (VALENT_EXTENSION (self));

  /* This is a cancelled event */
  if (valent_packet_check_field (packet, "isCancel"))
    {
      valent_telephony_plugin_restore_media_state (self);
      valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self),
                                              sender);
      return;
    }

  /* Adjust volume/pause media */
  valent_telephony_plugin_update_media_state (self, event);

  /* Notify user */
  notification = g_notification_new (sender);
  icon = valent_telephony_plugin_get_event_icon (packet, event);
  g_notification_set_icon (notification, icon);

  if (g_str_equal (event, "ringing"))
    {
      g_notification_set_body (notification, _("Incoming call"));
      valent_notification_add_device_button (notification,
                                             device,
                                             _("Mute"),
                                             "telephony.mute-call",
                                             NULL);
      g_notification_set_priority (notification,
                                   G_NOTIFICATION_PRIORITY_URGENT);
    }
  else if (g_str_equal (event, "talking"))
    {
      g_notification_set_body (notification, _("Ongoing call"));
    }

  valent_device_plugin_show_notification (VALENT_DEVICE_PLUGIN (self),
                                          sender,
                                          notification);
}

static void
valent_telephony_plugin_mute_call (ValentTelephonyPlugin *self)
{
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  packet = valent_packet_new ("kdeconnect.telephony.request_mute");
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * GActions
 */
static void
mute_call_action (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (user_data);

  g_assert (VALENT_IS_TELEPHONY_PLUGIN (self));

  valent_telephony_plugin_mute_call (self);
}

static const GActionEntry actions[] = {
    {"mute-call", mute_call_action, NULL, NULL, NULL}
};

/*
 * ValentDevicePlugin
 */
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

  valent_extension_toggle_actions (VALENT_EXTENSION (plugin), available);
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

  if (g_str_equal (type, "kdeconnect.telephony"))
    valent_telephony_plugin_handle_telephony (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_telephony_plugin_destroy (ValentObject *object)
{
  ValentTelephonyPlugin *self = VALENT_TELEPHONY_PLUGIN (object);

  g_clear_pointer (&self->prev_output, stream_state_free);
  g_clear_pointer (&self->prev_input, stream_state_free);

  VALENT_OBJECT_CLASS (valent_telephony_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_telephony_plugin_constructed (GObject *object)
{
  ValentDevicePlugin *plugin = VALENT_DEVICE_PLUGIN (object);

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);

  G_OBJECT_CLASS (valent_telephony_plugin_parent_class)->constructed (object);
}

static void
valent_telephony_plugin_class_init (ValentTelephonyPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->constructed = valent_telephony_plugin_constructed;

  vobject_class->destroy = valent_telephony_plugin_destroy;

  plugin_class->handle_packet = valent_telephony_plugin_handle_packet;
  plugin_class->update_state = valent_telephony_plugin_update_state;
}

static void
valent_telephony_plugin_init (ValentTelephonyPlugin *self)
{
}

