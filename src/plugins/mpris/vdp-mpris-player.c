// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "vdp-mpris-player"

#include "config.h"

#include <math.h>

#include <gio/gio.h>
#include <valent.h>

#include "vdp-mpris-player.h"
#include "valent-mpris-utils.h"


struct _VdpMprisPlayer
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

G_DEFINE_FINAL_TYPE (VdpMprisPlayer, vdp_mpris_player, VALENT_TYPE_MEDIA_PLAYER)


/*
 * ValentMediaPlayer
 */
static ValentMediaActions
vdp_mpris_player_get_flags (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  return self->flags;
}

static GVariant *
vdp_mpris_player_get_metadata (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  if (self->metadata)
    return g_variant_ref (self->metadata);

  return NULL;
}

static const char *
vdp_mpris_player_get_name (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  return self->name;
}

static double
vdp_mpris_player_get_position (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  if (self->state == VALENT_MEDIA_STATE_PLAYING)
    return self->position + (valent_mpris_get_time () - self->position_time);

  return self->position;
}

static void
vdp_mpris_player_set_position (ValentMediaPlayer *player,
                               double             position)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_get_repeat (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  return self->repeat;
}

static void
vdp_mpris_player_set_repeat (ValentMediaPlayer *player,
                             ValentMediaRepeat  repeat)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_get_shuffle (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  return self->shuffle;
}

static void
vdp_mpris_player_set_shuffle (ValentMediaPlayer *player,
                              gboolean           shuffle)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_get_state (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  return self->state;
}

static double
vdp_mpris_player_get_volume (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);

  return self->volume;
}

static void
vdp_mpris_player_set_volume (ValentMediaPlayer *player,
                             double             volume)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_next (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_pause (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_play (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_play_pause (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_previous (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_seek (ValentMediaPlayer *player,
                       double             offset)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_stop (ValentMediaPlayer *player)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (player);
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
vdp_mpris_player_request_album_art (VdpMprisPlayer *self,
                                    const char     *url,
                                    GVariantDict   *metadata)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  ValentContext *context = NULL;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;

  g_assert (VDP_IS_MPRIS_PLAYER (self));
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
vdp_mpris_player_update_flags (VdpMprisPlayer     *player,
                               ValentMediaActions  flags)
{
  g_assert (VDP_IS_MPRIS_PLAYER (player));

  if ((player->flags ^ flags) == 0)
    return;

  player->flags = flags;
  g_object_notify (G_OBJECT (player), "flags");
}

static void
vdp_mpris_player_update_metadata (VdpMprisPlayer *player,
                                  GVariant       *value)
{
  g_autoptr (GVariant) metadata = NULL;

  g_assert (VDP_IS_MPRIS_PLAYER (player));
  g_assert (value != NULL);

  if (player->metadata == value)
    return;

  metadata = g_steal_pointer (&player->metadata);
  player->metadata = g_variant_ref_sink (value);
  g_object_notify (G_OBJECT (player), "metadata");
}

static void
vdp_mpris_player_update_position (VdpMprisPlayer *player,
                                  int64_t         position)
{
  g_assert (VDP_IS_MPRIS_PLAYER (player));

  /* Convert milliseconds to seconds */
  player->position = position / 1000L;
  player->position_time = valent_mpris_get_time ();
  g_object_notify (G_OBJECT (player), "position");
}

static void
vdp_mpris_player_update_repeat (VdpMprisPlayer *player,
                                const char     *loop_status)
{
  ValentMediaRepeat repeat = VALENT_MEDIA_REPEAT_NONE;

  g_assert (VDP_IS_MPRIS_PLAYER (player));
  g_assert (loop_status != NULL);

  repeat = valent_mpris_repeat_from_string (loop_status);

  if (player->repeat == repeat)
    return;

  player->repeat = repeat;
  g_object_notify (G_OBJECT (player), "repeat");
}

static void
vdp_mpris_player_update_shuffle (VdpMprisPlayer *player,
                                 gboolean        shuffle)
{
  g_assert (VDP_IS_MPRIS_PLAYER (player));

  if (player->shuffle == shuffle)
    return;

  player->shuffle = shuffle;
  g_object_notify (G_OBJECT (player), "shuffle");
}

static void
vdp_mpris_player_update_state (VdpMprisPlayer *player,
                               const char     *playback_status)
{
  ValentMediaState state = VALENT_MEDIA_STATE_STOPPED;

  g_assert (VDP_IS_MPRIS_PLAYER (player));
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
vdp_mpris_player_update_volume (VdpMprisPlayer *player,
                                int64_t         volume)
{
  g_assert (VDP_IS_MPRIS_PLAYER (player));

  if (G_APPROX_VALUE (player->volume, volume / 100.0, 0.01))
    return;

  player->volume = CLAMP ((volume / 100.0), 0.0, 1.0);
  g_object_notify (G_OBJECT (player), "volume");
}

static void
on_device_state_changed (ValentDevice   *device,
                         GParamSpec     *pspec,
                         VdpMprisPlayer *self)
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
vdp_mpris_player_constructed (GObject *object)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (object);

  G_OBJECT_CLASS (vdp_mpris_player_parent_class)->constructed (object);

  self->device = valent_resource_get_source (VALENT_RESOURCE (self));
  g_signal_connect_object (self->device,
                           "notify::state",
                           G_CALLBACK (on_device_state_changed),
                           self,
                           G_CONNECT_DEFAULT);
}

static void
vdp_mpris_player_finalize (GObject *object)
{
  VdpMprisPlayer *self = VDP_MPRIS_PLAYER (object);

  g_clear_pointer (&self->name, g_free);
  g_clear_pointer (&self->metadata, g_variant_unref);

  G_OBJECT_CLASS (vdp_mpris_player_parent_class)->finalize (object);
}

static void
vdp_mpris_player_class_init (VdpMprisPlayerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMediaPlayerClass *player_class = VALENT_MEDIA_PLAYER_CLASS (klass);

  object_class->constructed = vdp_mpris_player_constructed;
  object_class->finalize = vdp_mpris_player_finalize;

  player_class->get_flags = vdp_mpris_player_get_flags;
  player_class->get_metadata = vdp_mpris_player_get_metadata;
  player_class->get_name = vdp_mpris_player_get_name;
  player_class->get_position = vdp_mpris_player_get_position;
  player_class->set_position = vdp_mpris_player_set_position;
  player_class->get_repeat = vdp_mpris_player_get_repeat;
  player_class->set_repeat = vdp_mpris_player_set_repeat;
  player_class->get_shuffle = vdp_mpris_player_get_shuffle;
  player_class->set_shuffle = vdp_mpris_player_set_shuffle;
  player_class->get_state = vdp_mpris_player_get_state;
  player_class->get_volume = vdp_mpris_player_get_volume;
  player_class->set_volume = vdp_mpris_player_set_volume;

  player_class->next = vdp_mpris_player_next;
  player_class->pause = vdp_mpris_player_pause;
  player_class->play = vdp_mpris_player_play;
  player_class->previous = vdp_mpris_player_previous;
  player_class->seek = vdp_mpris_player_seek;
  player_class->stop = vdp_mpris_player_stop;
}

static void
vdp_mpris_player_init (VdpMprisPlayer *self)
{
  self->name = g_strdup ("Media Player");
  self->volume = 1.0;
  self->state = VALENT_MEDIA_STATE_STOPPED;
}

/**
 * vdp_mpris_player_new:
 * @device: a `ValentDevice`
 *
 * Get the `VdpMprisPlayer` instance.
 *
 * Returns: (transfer full) (nullable): a `VdpMprisPlayer`
 */
VdpMprisPlayer *
vdp_mpris_player_new (ValentDevice *device)
{
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *iri = NULL;

  g_return_val_if_fail (VALENT_IS_DEVICE (device), NULL);

  context = valent_context_new (valent_device_get_context (device),
                                "plugin",
                                "systemvolume");
  iri = tracker_sparql_escape_uri_printf ("urn:valent:mixer:%s",
                                          valent_device_get_id (device));
  return g_object_new (VALENT_TYPE_MPRIS_PLAYER,
                       "iri",     iri,
                       "source",  device,
                       "title",   valent_device_get_name (device),
                       NULL);
}

/**
 * valent_media_player_update_packet:
 * @player: a `VdpMprisPlayer`
 * @packet: a KDE Connect packet
 *
 * A convenience method for updating the internal state of the player from a
 * `kdeconnect.mpris` packet.
 */
void
vdp_mpris_player_handle_packet (VdpMprisPlayer *player,
                                JsonNode       *packet)
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

  vdp_mpris_player_update_flags (player, flags);

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
    vdp_mpris_player_request_album_art (player, url, &metadata);

  vdp_mpris_player_update_metadata (player, g_variant_dict_end (&metadata));

  /* Playback Status */
  if (valent_packet_get_int (packet, "pos", &position))
    vdp_mpris_player_update_position (player, position);

  if (valent_packet_get_string (packet, "loopStatus", &loop_status))
    vdp_mpris_player_update_repeat (player, loop_status);

  if (valent_packet_get_boolean (packet, "isPlaying", &is_playing))
    vdp_mpris_player_update_state (player, is_playing ? "Playing" : "Paused");

  if (valent_packet_get_boolean (packet, "shuffle", &shuffle))
    vdp_mpris_player_update_shuffle (player, shuffle);

  if (valent_packet_get_int (packet, "volume", &volume))
    vdp_mpris_player_update_volume (player, volume);
}

/**
 * vdp_mpris_player_update_art:
 * @player: a `VdpMprisPlayer`
 * @file: a `GFile`
 *
 * Update the `mpris:artUrl` metadata field from @file.
 */
void
vdp_mpris_player_update_art (VdpMprisPlayer *player,
                             GFile          *file)
{
  GVariantDict dict;
  GVariant *metadata;
  g_autofree char *uri = NULL;

  g_assert (VDP_IS_MPRIS_PLAYER (player));
  g_assert (G_IS_FILE (file));

  uri = g_file_get_uri (file);

  g_variant_dict_init (&dict, player->metadata);
  g_variant_dict_insert (&dict, "mpris:artUrl", "s", uri);
  metadata = g_variant_dict_end (&dict);

  vdp_mpris_player_update_metadata (player, metadata);
}

/**
 * valent_media_player_update_name:
 * @player: a `VdpMprisPlayer`
 * @name: a name
 *
 * Set the user-visible name of the player to @identity.
 */
void
vdp_mpris_player_update_name (VdpMprisPlayer *player,
                              const char     *name)
{
  g_return_if_fail (VDP_IS_MPRIS_PLAYER (player));
  g_return_if_fail (name != NULL);

  if (g_set_str (&player->name, name))
    g_object_notify (G_OBJECT (player), "name");
}
