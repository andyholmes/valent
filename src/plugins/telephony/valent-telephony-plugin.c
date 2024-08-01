// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-telephony-plugin"

#include "config.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "valent-telephony-plugin.h"

/*
 * MediaState Helpers
 */
typedef struct
{
  ValentMixerStream *stream;
  unsigned int       current_level;
  unsigned int       current_muted : 1;
  unsigned int       original_level;
  unsigned int       original_muted : 1;
} StreamState;

static void
on_stream_changed (StreamState *state)
{
  g_signal_handlers_disconnect_by_data (valent_mixer_get_default (), state);
  g_clear_object (&state->stream);
}

static StreamState *
stream_state_new (ValentMixerStream *stream)
{
  StreamState *state;
  ValentMixerDirection direction;

  state = g_new0 (StreamState, 1);
  state->stream = g_object_ref (stream);
  state->original_level = valent_mixer_stream_get_level (stream);
  state->original_muted = valent_mixer_stream_get_muted (stream);
  state->current_level = state->original_level;
  state->current_muted = state->original_muted;

  direction = valent_mixer_stream_get_direction (stream);
  g_signal_connect_data (valent_mixer_get_default (),
                         direction == VALENT_MIXER_INPUT
                           ? "notify::default-input"
                           : "notify::default-output",
                         G_CALLBACK (on_stream_changed),
                         state, NULL,
                         G_CONNECT_SWAPPED);

  return state;
}

static void
stream_state_free (gpointer data)
{
  StreamState *state = data;

  if (state->stream != NULL)
    {
      g_signal_handlers_disconnect_by_data (valent_mixer_get_default (), state);
      g_clear_object (&state->stream);
    }
  g_free (state);
}

static inline void
stream_state_restore (gpointer data)
{
  StreamState *state = data;

  if (state->stream != NULL)
    {
      if (valent_mixer_stream_get_level (state->stream) == state->current_level)
        valent_mixer_stream_set_level (state->stream, state->original_level);

      if (valent_mixer_stream_get_muted (state->stream) == state->current_muted)
        valent_mixer_stream_set_muted (state->stream, state->original_muted);
    }
  stream_state_free (state);
}

static void
stream_state_update (StreamState *state,
                     int          level)
{
  if (state->stream == NULL)
    return;

  if (level == 0)
    {
      state->current_muted = TRUE;
      valent_mixer_stream_set_muted (state->stream, TRUE);
    }
  else if (level > 0)
    {
      state->current_level = level;
      valent_mixer_stream_set_level (state->stream, level);
    }
}

typedef struct
{
  GPtrArray *players;
  StreamState *speakers;
  StreamState *microphone;
} MediaState;

static void
on_player_changed (ValentMediaPlayer *player,
                   GParamSpec        *pspec,
                   GPtrArray         *players)
{
  /* The paused state may be deferred, but any other state stops tracking */
  if (valent_media_player_get_state (player) != VALENT_MEDIA_STATE_PAUSED)
    {
      g_signal_handlers_disconnect_by_data (player, players);
      g_ptr_array_remove (players, player);
    }
}

static MediaState *
media_state_new (void)
{
  MediaState *state;

  state = g_new0 (MediaState, 1);
  state->players = g_ptr_array_new ();

  return state;
}

static inline void
media_state_free (gpointer data)
{
  MediaState *state = data;

  g_signal_handlers_disconnect_by_data (valent_mixer_get_default (), state);
  g_clear_pointer (&state->players, g_ptr_array_unref);
  g_clear_pointer (&state->microphone, g_free);
  g_clear_pointer (&state->speakers, g_free);
  g_free (state);
}

static inline void
media_state_restore (gpointer data)
{
  MediaState *state = data;

  g_ptr_array_foreach (state->players, (void *)valent_media_player_play, NULL);
  g_clear_pointer (&state->players, g_ptr_array_unref);
  g_clear_pointer (&state->speakers, stream_state_restore);
  g_clear_pointer (&state->microphone, stream_state_restore);
  g_free (state);
}

static void
media_state_pause_players (MediaState *state)
{
  ValentMedia *media = valent_media_get_default ();
  unsigned int n_players = 0;

  n_players = g_list_model_get_n_items (G_LIST_MODEL (media));
  for (unsigned int i = 0; i < n_players; i++)
    {
      g_autoptr (ValentMediaPlayer) player = NULL;

      player = g_list_model_get_item (G_LIST_MODEL (media), i);

      /* Skip players already being tracked */
      if (g_ptr_array_find (state->players, player, NULL))
        continue;

      if (valent_media_player_get_state (player) != VALENT_MEDIA_STATE_PLAYING)
        continue;

      valent_media_player_pause (player);
      g_ptr_array_add (state->players, player);

      /* Stop tracking a player if its state changes or it's destroyed */
      g_signal_connect_data (player,
                             "notify::state",
                             G_CALLBACK (on_player_changed),
                             g_ptr_array_ref (state->players),
                             (void *)g_ptr_array_unref,
                             G_CONNECT_DEFAULT);
      g_signal_connect_data (player,
                             "destroy",
                             G_CALLBACK (g_ptr_array_remove),
                             g_ptr_array_ref (state->players),
                             (void *)g_ptr_array_unref,
                             G_CONNECT_SWAPPED);
    }
}

static void
media_state_update (MediaState *state,
                    int         output_level,
                    int         input_level,
                    gboolean    pause)
{
  ValentMixer *mixer = valent_mixer_get_default ();
  ValentMixerStream *stream = NULL;

  stream = valent_mixer_get_default_output (mixer);
  if (stream != NULL && output_level >= 0)
    {
      if (state->speakers == NULL)
        state->speakers = stream_state_new (stream);
      stream_state_update (state->speakers, output_level);
    }

  stream = valent_mixer_get_default_input (mixer);
  if (stream != NULL && input_level >= 0)
    {
      if (state->microphone == NULL)
        state->microphone = stream_state_new (stream);
      stream_state_update (state->microphone, input_level);
    }

  if (pause)
    media_state_pause_players (state);
}

/*
 * Plugin
 */
struct _ValentTelephonyPlugin
{
  ValentDevicePlugin  parent_instance;

  MediaState         *media_state;
};

G_DEFINE_FINAL_TYPE (ValentTelephonyPlugin, valent_telephony_plugin, VALENT_TYPE_DEVICE_PLUGIN)


static void
valent_telephony_plugin_update_media_state (ValentTelephonyPlugin *self,
                                            const char            *event)
{
  GSettings *settings = NULL;
  int output_level = -1;
  int input_level = -1;
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

  if (self->media_state == NULL)
    self->media_state = media_state_new ();
  media_state_update (self->media_state, output_level, input_level, pause);
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
      VALENT_NOTE ("ignoring \"%s\" event", event);
      return;
    }

  /* Ensure there is a string representing the sender, so it can be used as the
   * notification ID to handle interleaved events from multiple senders.
   *
   * Because we only support voice events (i.e. `ringing` and `talking`), we
   * can be certain that subsequent events from the same sender supersede
   * previous events, and replace the older notifications.
   */
  if (!valent_packet_get_string (packet, "contactName", &sender) &&
      !valent_packet_get_string (packet, "phoneNumber", &sender))
    {
      /* TRANSLATORS: An unknown caller, with no name or phone number */
      sender = C_("contact identity", "Unknown");
    }

  /* This is a cancelled event */
  if (valent_packet_check_field (packet, "isCancel"))
    {
      g_clear_pointer (&self->media_state, media_state_restore);
      valent_device_plugin_hide_notification (VALENT_DEVICE_PLUGIN (self),
                                              sender);
      return;
    }

  /* Adjust volume/pause media */
  valent_telephony_plugin_update_media_state (self, event);

  /* The notification plugin handles SMS/MMS and missed call notifications,
   * while the telephony plugin must handle incoming and ongoing calls.
   */
  notification = g_notification_new (sender);
  icon = valent_telephony_plugin_get_event_icon (packet, event);
  g_notification_set_icon (notification, icon);

  if (g_str_equal (event, "ringing"))
    {
      ValentDevice *device = NULL;

      /* TRANSLATORS: The phone is ringing */
      g_notification_set_body (notification, _("Incoming call"));
      device = valent_extension_get_object (VALENT_EXTENSION (self));
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
      /* TRANSLATORS: The phone has been answered */
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

  /* Clear the media state, but don't restore it as there may still be an
   * event in progress. */
  if (!available)
    g_clear_pointer (&self->media_state, media_state_free);

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

  g_clear_pointer (&self->media_state, media_state_free);

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

