// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-plugin"

#include "config.h"

#include <math.h>

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <valent.h>

#include "vdp-mpris-adapter.h"
#include "vdp-mpris-player.h"
#include "valent-mpris-utils.h"

#include "valent-mpris-plugin.h"

struct _ValentMprisPlugin
{
  ValentDevicePlugin  parent_instance;

  ValentMedia        *media;
  unsigned int        media_watch : 1;
  ValentMediaAdapter *adapter;
  GPtrArray          *players;

  GHashTable         *transfers;
  GHashTable         *pending;
  unsigned int        pending_list : 1;
  unsigned int        flush_id;
};

G_DEFINE_FINAL_TYPE (ValentMprisPlugin, valent_mpris_plugin, VALENT_TYPE_DEVICE_PLUGIN)

static void valent_mpris_plugin_send_player_info    (ValentMprisPlugin *self,
                                                     ValentMediaPlayer *player,
                                                     gboolean           now_playing,
                                                     gboolean           volume);
static void valent_mpris_plugin_send_player_list    (ValentMprisPlugin *self);


static gpointer
valent_mpris_plugin_lookup_player (ValentMprisPlugin *self,
                                   const char        *name)
{
  for (unsigned int i = 0; i < self->players->len; i++)
    {
      ValentMediaPlayer *player = g_ptr_array_index (self->players, i);
      if (g_strcmp0 (valent_media_player_get_name (player), name) == 0)
        return player;
    }

  return NULL;
}

/*
 * Local Players
 */
static void
send_album_art_cb (ValentTransfer    *transfer,
                   GAsyncResult      *result,
                   ValentMprisPlugin *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *id = NULL;

  g_assert (VALENT_IS_TRANSFER (transfer));

  if (!valent_transfer_execute_finish (transfer, result, &error))
    g_debug ("%s(): %s", G_STRFUNC, error->message);

  id = valent_transfer_dup_id (transfer);
  g_hash_table_remove (self->transfers, id);
}

static void
valent_mpris_plugin_send_album_art (ValentMprisPlugin *self,
                                    ValentMediaPlayer *player,
                                    const char        *requested_uri)
{
  g_autoptr (GVariant) metadata = NULL;
  const char *real_uri;
  g_autoptr (GFile) real_file = NULL;
  g_autoptr (GFile) requested_file = NULL;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (ValentTransfer) transfer = NULL;
  ValentDevice *device;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  /* Ignore concurrent requests */
  if (g_hash_table_contains (self->transfers, requested_uri))
    return;

  /* Check player and URL are safe */
  if ((metadata = valent_media_player_get_metadata (player)) == NULL ||
      !g_variant_lookup (metadata, "mpris:artUrl", "&s", &real_uri))
    {
      g_warning ("Album art request \"%s\" for track without album art",
                 requested_uri);
      return;
    }

  /* Compare normalized URLs */
  requested_file = g_file_new_for_uri (requested_uri);
  real_file = g_file_new_for_uri (real_uri);

  if (!g_file_equal (requested_file, real_file))
    {
      g_warning ("Album art request \"%s\" doesn't match current track \"%s\"",
                 requested_uri, real_uri);
      return;
    }

  /* Build the payload packet */
  valent_packet_init (&builder, "kdeconnect.mpris");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, valent_media_player_get_name (player));
  json_builder_set_member_name (builder, "albumArtUrl");
  json_builder_add_string_value (builder, requested_uri);
  json_builder_set_member_name (builder, "transferringAlbumArt");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  /* Start the transfer */
  device = valent_resource_get_source (VALENT_RESOURCE (self));
  transfer = valent_device_transfer_new (device, packet, real_file);

  g_hash_table_insert (self->transfers,
                       g_strdup (requested_uri),
                       g_object_ref (transfer));

  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)send_album_art_cb,
                           self);
}

static gboolean
valent_mpris_plugin_flush (gpointer data)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (data);
  GHashTableIter iter;
  ValentMediaPlayer *player;

  g_hash_table_iter_init (&iter, self->pending);
  while (g_hash_table_iter_next (&iter, (void **)&player, NULL))
    {
      valent_mpris_plugin_send_player_info (self, player, TRUE, TRUE);
      g_hash_table_iter_remove (&iter);
    }

  if (self->pending_list)
    {
      valent_mpris_plugin_send_player_list (self);
      self->pending_list = FALSE;
    }

  self->flush_id = 0;

  return G_SOURCE_REMOVE;
}

static void
on_player_seeked (ValentMediaPlayer *player,
                  double             position,
                  ValentMprisPlugin *self)
{
  const char *name;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  name = valent_media_player_get_name (player);

  /* Convert seconds to milliseconds */
  valent_packet_init (&builder, "kdeconnect.mpris");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, name);
  json_builder_set_member_name (builder, "pos");
  json_builder_add_int_value (builder, (int64_t)(position * 1000L));
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
on_player_changed (ValentMediaPlayer *player,
                   GParamSpec        *pspec,
                   ValentMprisPlugin *self)
{
  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  if (g_str_equal (pspec->name, "position"))
    {
      double position = 0.0;

      position = valent_media_player_get_position (player);
      on_player_seeked (player, position, self);
    }
  else
    {
      g_hash_table_add (self->pending, player);
      if (self->flush_id == 0)
        self->flush_id = g_idle_add (valent_mpris_plugin_flush, self);
    }
}

static void
on_player_destroy (ValentMediaPlayer *player,
                   ValentMprisPlugin *self)
{
  g_hash_table_remove (self->pending, player);
  g_ptr_array_remove (self->players, player);

  self->pending_list = TRUE;
  if (self->flush_id == 0)
    self->flush_id = g_idle_add (valent_mpris_plugin_flush, self);
}

static void
on_players_changed (GListModel        *list,
                    unsigned int       position,
                    unsigned int       removed,
                    unsigned int       added,
                    ValentMprisPlugin *self)
{
  for (unsigned int i = 0; i < added; i++)
    {
      g_autoptr (ValentMediaPlayer) player = NULL;

      player = g_list_model_get_item (list, position + i);
      g_signal_connect_object (player,
                               "notify",
                               G_CALLBACK (on_player_changed),
                               self,
                               G_CONNECT_DEFAULT);
      g_signal_connect_object (player,
                               "destroy",
                               G_CALLBACK (on_player_destroy),
                               self,
                               G_CONNECT_DEFAULT);
      g_ptr_array_add (self->players, player);
    }

  self->pending_list = TRUE;
  if (self->flush_id == 0)
    self->flush_id = g_idle_add (valent_mpris_plugin_flush, self);
}

static void
on_adapters_changed (GListModel        *list,
                     unsigned int       position,
                     unsigned int       removed,
                     unsigned int       added,
                     ValentMprisPlugin *self)
{
  g_assert (VALENT_IS_MEDIA (list));
  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  for (unsigned int i = 0; i < added; i++)
    {
      g_autoptr (ValentMediaAdapter) adapter = NULL;

      adapter = g_list_model_get_item (list, position + i);
      if (VDP_IS_MPRIS_ADAPTER (adapter))
        continue;

      g_signal_connect_object (adapter,
                               "items-changed",
                               G_CALLBACK (on_players_changed),
                               self,
                               G_CONNECT_DEFAULT);
      on_players_changed (G_LIST_MODEL (adapter),
                          0,
                          0,
                          g_list_model_get_n_items (G_LIST_MODEL (adapter)),
                          self);
    }
}

static void
valent_mpris_plugin_watch_media (ValentMprisPlugin *self,
                                 gboolean           watch)
{
  ValentMedia *media = valent_media_get_default ();

  if (self->media_watch == watch)
    return;

  if (watch)
    {
      g_signal_connect_object (media,
                               "items-changed",
                               G_CALLBACK (on_adapters_changed),
                               self,
                               G_CONNECT_DEFAULT);
      on_adapters_changed (G_LIST_MODEL (media),
                           0,
                           0,
                           g_list_model_get_n_items (G_LIST_MODEL (media)),
                           self);

      if (self->adapter == NULL)
        {
          ValentDevice *device = NULL;

          device = valent_resource_get_source (VALENT_RESOURCE (self));
          self->adapter = vdp_mpris_adapter_new (device);
          valent_component_export_adapter (VALENT_COMPONENT (media),
                                           VALENT_EXTENSION (self->adapter));
        }
    }
  else
    {
      unsigned int n_adapters = 0;

      n_adapters = g_list_model_get_n_items (G_LIST_MODEL (media));
      for (unsigned int i = 0; i < n_adapters; i++)
        {
          g_autoptr (ValentMediaAdapter) adapter = NULL;

          adapter = g_list_model_get_item (G_LIST_MODEL (media), i);
          g_signal_handlers_disconnect_by_data (adapter, self);
        }

      for (unsigned int i = 0; i < self->players->len; i++)
        {
          ValentMediaPlayer *player = NULL;

          player = g_ptr_array_index (self->players, i);
          g_signal_handlers_disconnect_by_data (player, self);
        }

      g_hash_table_remove_all (self->pending);
      g_ptr_array_remove_range (self->players, 0, self->players->len);
      g_clear_handle_id (&self->flush_id, g_source_remove);
      g_signal_handlers_disconnect_by_data (media, self);

      if (self->adapter != NULL)
        {
          valent_component_unexport_adapter (VALENT_COMPONENT (media),
                                             VALENT_EXTENSION (self->adapter));
          g_clear_object (&self->adapter);
        }
    }

  self->media_watch = watch;
}

static void
valent_mpris_plugin_handle_action (ValentMprisPlugin *self,
                                   ValentMediaPlayer *player,
                                   const char        *action)
{
  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));
  g_assert (action && *action);

  if (g_str_equal (action, "Next"))
    valent_media_player_next (player);

  else if (g_str_equal (action, "Pause"))
    valent_media_player_pause (player);

  else if (g_str_equal (action, "Play"))
    valent_media_player_play (player);

  else if (g_str_equal (action, "PlayPause"))
    valent_mpris_play_pause (player);

  else if (g_str_equal (action, "Previous"))
    valent_media_player_previous (player);

  else if (g_str_equal (action, "Stop"))
    valent_media_player_stop (player);

  else
    g_warning ("%s(): Unknown action: %s", G_STRFUNC, action);
}

static void
valent_mpris_plugin_handle_mpris_request (ValentMprisPlugin *self,
                                          JsonNode          *packet)
{
  ValentMediaPlayer *player = NULL;
  const char *name;
  const char *action;
  const char *url;
  int64_t offset_us;
  int64_t position;
  gboolean request_now_playing;
  gboolean request_volume;
  const char *loop_status;
  ValentMediaRepeat repeat;
  gboolean shuffle;
  int64_t volume;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  /* Start by checking for a player */
  if (valent_packet_get_string (packet, "player", &name))
    player = valent_mpris_plugin_lookup_player (self, name);

  if (player == NULL || valent_packet_check_field (packet, "requestPlayerList"))
    {
      valent_mpris_plugin_send_player_list (self);
      return;
    }

  /* A request for a player's status */
  request_now_playing = valent_packet_check_field (packet, "requestNowPlaying");
  request_volume = valent_packet_check_field (packet, "requestVolume");

  if (request_now_playing || request_volume)
    valent_mpris_plugin_send_player_info (self,
                                          player,
                                          request_now_playing,
                                          request_volume);

  if (valent_packet_get_string (packet, "action", &action))
    valent_mpris_plugin_handle_action (self, player, action);

  /* A request to change the relative position (microseconds to seconds) */
  if (valent_packet_get_int (packet, "Seek", &offset_us))
    valent_media_player_seek (player, offset_us / G_TIME_SPAN_SECOND);

  /* A request to change the absolute position (milliseconds to seconds) */
  if (valent_packet_get_int (packet, "SetPosition", &position))
    valent_media_player_set_position (player, position / 1000L);

  if (valent_packet_get_string (packet, "setLoopStatus", &loop_status))
    {
      repeat = valent_mpris_repeat_from_string (loop_status);
      valent_media_player_set_repeat (player, repeat);
    }

  if (valent_packet_get_boolean (packet, "setShuffle", &shuffle))
    valent_media_player_set_shuffle (player, shuffle);

  if (valent_packet_get_int (packet, "setVolume", &volume))
    valent_media_player_set_volume (player, volume / 100.0);

  if (valent_packet_get_string (packet, "albumArtUrl", &url))
    valent_mpris_plugin_send_album_art (self, player, url);
}

static void
valent_mpris_plugin_send_player_info (ValentMprisPlugin *self,
                                      ValentMediaPlayer *player,
                                      gboolean           request_now_playing,
                                      gboolean           request_volume)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;
  const char *name;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  name = valent_media_player_get_name (player);

  valent_packet_init (&builder, "kdeconnect.mpris");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, name);

  /* Player State & Metadata */
  if (request_now_playing)
    {
      ValentMediaActions flags;
      ValentMediaRepeat repeat;
      gboolean is_playing;
      double position;
      gboolean shuffle;
      const char *loop_status = "None";
      g_autoptr (GVariant) metadata = NULL;
      g_autofree char *artist = NULL;
      const char *title = NULL;

      /* Player State */
      flags = valent_media_player_get_flags (player);
      json_builder_set_member_name (builder, "canPause");
      json_builder_add_boolean_value (builder, (flags & VALENT_MEDIA_ACTION_PAUSE) != 0);
      json_builder_set_member_name (builder, "canPlay");
      json_builder_add_boolean_value (builder, (flags & VALENT_MEDIA_ACTION_PLAY) != 0);
      json_builder_set_member_name (builder, "canGoNext");
      json_builder_add_boolean_value (builder, (flags & VALENT_MEDIA_ACTION_NEXT) != 0);
      json_builder_set_member_name (builder, "canGoPrevious");
      json_builder_add_boolean_value (builder,(flags & VALENT_MEDIA_ACTION_PREVIOUS) != 0);
      json_builder_set_member_name (builder, "canSeek");
      json_builder_add_boolean_value (builder, (flags & VALENT_MEDIA_ACTION_SEEK) != 0);

      repeat = valent_media_player_get_repeat (player);
      loop_status = valent_mpris_repeat_to_string (repeat);
      json_builder_set_member_name (builder, "loopStatus");
      json_builder_add_string_value (builder, loop_status);

      shuffle = valent_media_player_get_shuffle (player);
      json_builder_set_member_name (builder, "shuffle");
      json_builder_add_boolean_value (builder, shuffle);

      is_playing = valent_media_player_get_state (player) == VALENT_MEDIA_STATE_PLAYING;
      json_builder_set_member_name (builder, "isPlaying");
      json_builder_add_boolean_value (builder, is_playing);

      /* Convert seconds to milliseconds */
      position = valent_media_player_get_position (player);
      json_builder_set_member_name (builder, "pos");
      json_builder_add_int_value (builder, (int64_t)(position * 1000L));

      /* Track Metadata
       *
       * See: https://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata/
       */
      if ((metadata = valent_media_player_get_metadata (player)) != NULL)
        {
          g_autofree const char **artists = NULL;
          int64_t length_us;
          const char *art_url;
          const char *album;

          if (g_variant_lookup (metadata, "xesam:artist", "^a&s", &artists) &&
              artists[0] != NULL && *artists[0] != '\0')
            {
              artist = g_strjoinv (", ", (char **)artists);
              json_builder_set_member_name (builder, "artist");
              json_builder_add_string_value (builder, artist);
            }

          if (g_variant_lookup (metadata, "xesam:title", "&s", &title) &&
              *title != '\0')
            {
              json_builder_set_member_name (builder, "title");
              json_builder_add_string_value (builder, title);
            }

          if (g_variant_lookup (metadata, "xesam:album", "&s", &album) &&
              *album != '\0')
            {
              json_builder_set_member_name (builder, "album");
              json_builder_add_string_value (builder, album);
            }

          /* Convert microseconds to milliseconds */
          if (g_variant_lookup (metadata, "mpris:length", "x", &length_us))
            {
              json_builder_set_member_name (builder, "length");
              json_builder_add_int_value (builder, length_us / 1000L);
            }

          if (g_variant_lookup (metadata, "mpris:artUrl", "&s", &art_url))
            {
              json_builder_set_member_name (builder, "albumArtUrl");
              json_builder_add_string_value (builder, art_url);
            }
        }
    }

  /* Volume Level */
  if (request_volume)
    {
      int64_t level;

      level = (int64_t)floor (valent_media_player_get_volume (player) * 100);
      json_builder_set_member_name (builder, "volume");
      json_builder_add_int_value (builder, level);
    }

  /* Send Response */
  response = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), response);
}

static void
valent_mpris_plugin_send_player_list (ValentMprisPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.mpris");

  /* Player List */
  json_builder_set_member_name (builder, "playerList");
  json_builder_begin_array (builder);

  for (unsigned int i = 0; i < self->players->len; i++)
    {
      ValentMediaPlayer *player = g_ptr_array_index (self->players, i);
      const char *name = valent_media_player_get_name (player);

      if (name != NULL)
        json_builder_add_string_value (builder, name);
    }

  json_builder_end_array (builder);

  /* Indicate that the remote device may send us album art payloads */
  json_builder_set_member_name (builder, "supportAlbumArtPayload");
  json_builder_add_boolean_value (builder, TRUE);

  packet = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

/*
 * ValentDevicePlugin
 */
static void
valent_mpris_plugin_update_state (ValentDevicePlugin *plugin,
                                  ValentDeviceState   state)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  valent_mpris_plugin_watch_media (self, available);
}

static void
valent_mpris_plugin_handle_packet (ValentDevicePlugin *plugin,
                                   const char         *type,
                                   JsonNode           *packet)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);

  g_assert (VALENT_IS_MPRIS_PLUGIN (plugin));
  g_assert (type != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  if (g_str_equal (type, "kdeconnect.mpris"))
    vdp_mpris_adapter_handle_packet (VDP_MPRIS_ADAPTER (self->adapter), packet);
  else if (g_str_equal (type, "kdeconnect.mpris.request"))
    valent_mpris_plugin_handle_mpris_request (self, packet);
  else
    g_assert_not_reached ();
}

/*
 * ValentObject
 */
static void
valent_mpris_plugin_destroy (ValentObject *object)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (object);

  valent_mpris_plugin_watch_media (self, FALSE);
  g_clear_pointer (&self->players, g_ptr_array_unref);
  g_clear_pointer (&self->pending, g_hash_table_unref);
  g_clear_pointer (&self->transfers, g_hash_table_unref);

  VALENT_OBJECT_CLASS (valent_mpris_plugin_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mpris_plugin_class_init (ValentMprisPluginClass *klass)
{
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  vobject_class->destroy = valent_mpris_plugin_destroy;

  plugin_class->handle_packet = valent_mpris_plugin_handle_packet;
  plugin_class->update_state = valent_mpris_plugin_update_state;
}

static void
valent_mpris_plugin_init (ValentMprisPlugin *self)
{
  self->transfers = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           g_object_unref);
  self->players = g_ptr_array_new ();
  self->pending = g_hash_table_new (NULL, NULL);
}

