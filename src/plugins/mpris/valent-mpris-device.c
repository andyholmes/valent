// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-device"

#include "config.h"

#include <math.h>

#include <gio/gio.h>
#include <valent.h>

#include "valent-mpris-device.h"
#include "valent-mpris-utils.h"


struct _ValentMprisDevice
{
  ValentMediaPlayer   parent_instance;

  ValentDevice       *device;

  ValentMediaActions  flags;
  char               *name;
  GVariant           *metadata;
  double              position;
  double              position_time;
  ValentMediaRepeat   repeat;
  unsigned int        shuffle : 1;
  ValentMediaState    state;
  double              volume;
};

G_DEFINE_FINAL_TYPE (ValentMprisDevice, valent_mpris_device, VALENT_TYPE_MEDIA_PLAYER)


/*
 * ValentMediaPlayer
 */
static ValentMediaActions
valent_mpris_device_get_flags (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  return self->flags;
}

static GVariant *
valent_mpris_device_get_metadata (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  if (self->metadata)
    return g_variant_ref (self->metadata);

  return NULL;
}

static const char *
valent_mpris_device_get_name (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  return self->name;
}

static double
valent_mpris_device_get_position (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  if (self->state == VALENT_MEDIA_STATE_PLAYING)
    return self->position + (valent_mpris_get_time () - self->position_time);

  return self->position;
}

static void
valent_mpris_device_set_position (ValentMediaPlayer *player,
                                  double             position)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  /* Convert seconds to milliseconds */
  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "SetPosition");
  json_builder_add_int_value (builder, (int64_t)(position * 1000L));
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static ValentMediaRepeat
valent_mpris_device_get_repeat (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  return self->repeat;
}

static void
valent_mpris_device_set_repeat (ValentMediaPlayer *player,
                                ValentMediaRepeat  repeat)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  const char *loop_status = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  loop_status = valent_mpris_repeat_to_string (repeat);

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "setLoopStatus");
  json_builder_add_string_value (builder, loop_status);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static gboolean
valent_mpris_device_get_shuffle (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  return self->shuffle;
}

static void
valent_mpris_device_set_shuffle (ValentMediaPlayer *player,
                                 gboolean           shuffle)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "setShuffle");
  json_builder_add_boolean_value (builder, shuffle);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static ValentMediaState
valent_mpris_device_get_state (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  return self->state;
}

static double
valent_mpris_device_get_volume (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);

  return self->volume;
}

static void
valent_mpris_device_set_volume (ValentMediaPlayer *player,
                                double             volume)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "setVolume");
  json_builder_add_int_value (builder, (int64_t)floor (volume * 100));
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static void
valent_mpris_device_next (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "Next");
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static void
valent_mpris_device_pause (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "Pause");
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static void
valent_mpris_device_play (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "Play");
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

#if 0
static void
valent_mpris_device_play_pause (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "PlayPause");
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}
#endif

static void
valent_mpris_device_previous (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "Previous");
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static void
valent_mpris_device_seek (ValentMediaPlayer *player,
                          double             offset)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  /* Convert seconds to microseconds */
  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "Seek");
  json_builder_add_int_value (builder, (int64_t)(offset * G_TIME_SPAN_SECOND));
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static void
valent_mpris_device_stop (ValentMediaPlayer *player)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (player);
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "action");
  json_builder_add_string_value (builder, "Stop");
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

static void
valent_mpris_device_request_album_art (ValentMprisDevice *self,
                                       const char        *url,
                                       GVariantDict      *metadata)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  ValentContext *context = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;

  g_assert (VALENT_IS_MPRIS_DEVICE (self));
  g_assert (url != NULL && *url != '\0');
  g_assert (metadata != NULL);

  context = valent_device_get_context (self->device);
  filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
  file = valent_context_get_cache_file (context, filename);

  /* If the album art has been cached, update the metadata dictionary */
  if (g_file_query_exists (file, NULL))
    {
      g_autofree char *art_url = NULL;

      art_url = g_file_get_uri (file);
      g_variant_dict_insert (metadata, "mpris:artUrl", "s", art_url);

      return;
    }

  /* Request the album art payload */
  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, self->name);
  json_builder_set_member_name (builder, "albumArtUrl");
  json_builder_add_string_value (builder, url);
  packet = valent_packet_end (&builder);

  valent_device_send_packet (self->device, packet, NULL, NULL, NULL);
}

/*
 * Private
 */
static void
valent_mpris_device_update_flags (ValentMprisDevice  *player,
                                  ValentMediaActions  flags)
{
  g_assert (VALENT_IS_MPRIS_DEVICE (player));

  if ((player->flags ^ flags) == 0)
    return;

  player->flags = flags;
  g_object_notify (G_OBJECT (player), "flags");
}

static void
valent_mpris_device_update_metadata (ValentMprisDevice *player,
                                     GVariant          *value)
{
  g_autoptr (GVariant) metadata = NULL;

  g_assert (VALENT_IS_MPRIS_DEVICE (player));
  g_assert (value != NULL);

  if (player->metadata == value)
    return;

  metadata = g_steal_pointer (&player->metadata);
  player->metadata = g_variant_ref_sink (value);
  g_object_notify (G_OBJECT (player), "metadata");
}

static void
valent_mpris_device_update_position (ValentMprisDevice *player,
                                     int64_t            position)
{
  g_assert (VALENT_IS_MPRIS_DEVICE (player));

  /* Convert milliseconds to seconds */
  player->position = position / 1000L;
  player->position_time = valent_mpris_get_time ();
  g_object_notify (G_OBJECT (player), "position");
}

static void
valent_mpris_device_update_repeat (ValentMprisDevice *player,
                                   const char        *loop_status)
{
  ValentMediaRepeat repeat = VALENT_MEDIA_REPEAT_NONE;

  g_assert (VALENT_IS_MPRIS_DEVICE (player));
  g_assert (loop_status != NULL);

  repeat = valent_mpris_repeat_from_string (loop_status);

  if (player->repeat == repeat)
    return;

  player->repeat = repeat;
  g_object_notify (G_OBJECT (player), "repeat");
}

static void
valent_mpris_device_update_shuffle (ValentMprisDevice *player,
                                    gboolean           shuffle)
{
  g_assert (VALENT_IS_MPRIS_DEVICE (player));

  if (player->shuffle == shuffle)
    return;

  player->shuffle = shuffle;
  g_object_notify (G_OBJECT (player), "shuffle");
}

static void
valent_mpris_device_update_state (ValentMprisDevice *player,
                                  const char        *playback_status)
{
  ValentMediaState state = VALENT_MEDIA_STATE_STOPPED;

  g_assert (VALENT_IS_MPRIS_DEVICE (player));
  g_assert (playback_status != NULL);

  state = valent_mpris_state_from_string (playback_status);

  if (player->state == state)
    return;

  player->state = state;

  if (player->state == VALENT_MEDIA_STATE_STOPPED)
    {
      player->position = 0.0;
      player->position_time = 0;
      g_object_notify (G_OBJECT (player), "position");
    }

  g_object_notify (G_OBJECT (player), "state");
}

static void
valent_mpris_device_update_volume (ValentMprisDevice *player,
                                   int64_t            volume)
{
  g_assert (VALENT_IS_MPRIS_DEVICE (player));

  if (G_APPROX_VALUE (player->volume, volume / 100.0, 0.01))
    return;

  player->volume = CLAMP ((volume / 100.0), 0.0, 1.0);
  g_object_notify (G_OBJECT (player), "volume");
}

static void
on_device_state_changed (ValentDevice      *device,
                         GParamSpec        *pspec,
                         ValentMprisDevice *self)
{
#if 0
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;
  gboolean available;

  state = valent_device_get_state (device);
  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;
#endif
}

/*
 * GObject
 */
static void
valent_mpris_device_constructed (GObject *object)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (object);

  G_OBJECT_CLASS (valent_mpris_device_parent_class)->constructed (object);

  self->device = valent_resource_get_source (VALENT_RESOURCE (self));
  g_signal_connect_object (self->device,
                           "notify::state",
                           G_CALLBACK (on_device_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
}

static void
valent_mpris_device_finalize (GObject *object)
{
  ValentMprisDevice *self = VALENT_MPRIS_DEVICE (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->metadata, g_variant_unref);

  G_OBJECT_CLASS (valent_mpris_device_parent_class)->finalize (object);
}

static void
valent_mpris_device_class_init (ValentMprisDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaPlayerClass *player_class = VALENT_MEDIA_PLAYER_CLASS (klass);

  object_class->constructed = valent_mpris_device_constructed;
  object_class->finalize = valent_mpris_device_finalize;

  player_class->get_flags = valent_mpris_device_get_flags;
  player_class->get_metadata = valent_mpris_device_get_metadata;
  player_class->get_name = valent_mpris_device_get_name;
  player_class->get_position = valent_mpris_device_get_position;
  player_class->set_position = valent_mpris_device_set_position;
  player_class->get_repeat = valent_mpris_device_get_repeat;
  player_class->set_repeat = valent_mpris_device_set_repeat;
  player_class->get_shuffle = valent_mpris_device_get_shuffle;
  player_class->set_shuffle = valent_mpris_device_set_shuffle;
  player_class->get_state = valent_mpris_device_get_state;
  player_class->get_volume = valent_mpris_device_get_volume;
  player_class->set_volume = valent_mpris_device_set_volume;

  player_class->next = valent_mpris_device_next;
  player_class->pause = valent_mpris_device_pause;
  player_class->play = valent_mpris_device_play;
  player_class->previous = valent_mpris_device_previous;
  player_class->seek = valent_mpris_device_seek;
  player_class->stop = valent_mpris_device_stop;
}

static void
valent_mpris_device_init (ValentMprisDevice *self)
{
  self->name = g_strdup ("Media Player");
  self->volume = 1.0;
  self->state = VALENT_MEDIA_STATE_STOPPED;
}

/**
 * valent_mpris_device_new:
 * @device: a `ValentDevice`
 *
 * Get the `ValentMprisDevice` instance.
 *
 * Returns: (transfer full) (nullable): a `ValentMprisDevice`
 */
ValentMprisDevice *
valent_mpris_device_new (ValentDevice *device)
{
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *iri = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  context = valent_context_new (valent_device_get_context (device),
                                "plugin",
                                "systemvolume");
  iri = tracker_sparql_escape_uri_printf ("urn:valent:mixer:%s",
                                          valent_device_get_id (device));
  return g_object_new (VALENT_TYPE_MPRIS_DEVICE,
                       "iri",     iri,
                       "source",  device,
                       "title",   valent_device_get_name (device),
                       NULL);
}

/**
 * valent_media_player_update_packet:
 * @player: a `ValentMprisDevice`
 * @packet: a KDE Connect packet
 *
 * A convenience method for updating the internal state of the player from a
 * `kdeconnect.mpris` packet.
 */
void
valent_mpris_device_handle_packet (ValentMprisDevice  *player,
                                   JsonNode           *packet)
{
  const char *url;
  ValentMediaActions flags = VALENT_MEDIA_ACTION_NONE;
  GVariantDict metadata;
  const char *artist, *title, *album;
  int64_t length, position;
  const char *loop_status = NULL;
  gboolean shuffle = FALSE;
  gboolean is_playing;
  int64_t volume;

  /* Flags (available actions) */
  if (valent_packet_check_field (packet, "canGoNext"))
    flags |= VALENT_MEDIA_ACTION_NEXT;

  if (valent_packet_check_field (packet, "canGoPrevious"))
    flags |= VALENT_MEDIA_ACTION_PREVIOUS;

  if (valent_packet_check_field (packet, "canPause"))
    flags |= VALENT_MEDIA_ACTION_PAUSE;

  if (valent_packet_check_field (packet, "canPlay"))
    flags |= VALENT_MEDIA_ACTION_PLAY;

  if (valent_packet_check_field (packet, "canSeek"))
    flags |= VALENT_MEDIA_ACTION_SEEK;

  valent_mpris_device_update_flags (player, flags);

  /* Metadata */
  g_variant_dict_init (&metadata, NULL);

  if (valent_packet_get_string (packet, "artist", &artist))
    {
      g_auto (GStrv) artists = NULL;
      GVariant *value;

      artists = g_strsplit (artist, ",", -1);
      value = g_variant_new_strv ((const char * const *)artists, -1);
      g_variant_dict_insert_value (&metadata, "xesam:artist", value);
    }

  if (valent_packet_get_string (packet, "title", &title))
    g_variant_dict_insert (&metadata, "xesam:title", "s", title);

  if (valent_packet_get_string (packet, "album", &album))
    g_variant_dict_insert (&metadata, "xesam:album", "s", album);

  /* Convert milliseconds to microseconds */
  if (valent_packet_get_int (packet, "length", &length))
    g_variant_dict_insert (&metadata, "mpris:length", "x", length * 1000L);

  if (valent_packet_get_string (packet, "albumArtUrl", &url))
    valent_mpris_device_request_album_art (player, url, &metadata);

  valent_mpris_device_update_metadata (player, g_variant_dict_end (&metadata));

  /* Playback Status */
  if (valent_packet_get_int (packet, "pos", &position))
    valent_mpris_device_update_position (player, position);

  if (valent_packet_get_string (packet, "loopStatus", &loop_status))
    valent_mpris_device_update_repeat (player, loop_status);

  if (valent_packet_get_boolean (packet, "isPlaying", &is_playing))
    valent_mpris_device_update_state (player, is_playing ? "Playing" : "Paused");

  if (valent_packet_get_boolean (packet, "shuffle", &shuffle))
    valent_mpris_device_update_shuffle (player, shuffle);

  if (valent_packet_get_int (packet, "volume", &volume))
    valent_mpris_device_update_volume (player, volume);
}

/**
 * valent_mpris_device_update_art:
 * @player: a `ValentMprisDevice`
 * @file: a `GFile`
 *
 * Update the `mpris:artUrl` metadata field from @file.
 */
void
valent_mpris_device_update_art (ValentMprisDevice *player,
                                GFile             *file)
{
  GVariantDict dict;
  GVariant *metadata;
  g_autofree char *uri = NULL;

  g_assert (VALENT_IS_MPRIS_DEVICE (player));
  g_assert (G_IS_FILE (file));

  uri = g_file_get_uri (file);

  g_variant_dict_init (&dict, player->metadata);
  g_variant_dict_insert (&dict, "mpris:artUrl", "s", uri);
  metadata = g_variant_dict_end (&dict);

  valent_mpris_device_update_metadata (player, metadata);
}

/**
 * valent_media_player_update_name:
 * @player: a `ValentMprisDevice`
 * @name: a name
 *
 * Set the user-visible name of the player to @identity.
 */
void
valent_mpris_device_update_name (ValentMprisDevice *player,
                                 const char        *name)
{
  g_return_if_fail (VALENT_IS_MPRIS_DEVICE (player));
  g_return_if_fail (name != NULL);

  if (g_set_str (&player->name, name))
    g_object_notify (G_OBJECT (player), "name");
}
