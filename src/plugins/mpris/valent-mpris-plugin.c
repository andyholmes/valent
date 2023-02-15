// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-plugin"

#include "config.h"

#include <math.h>

#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-device.h>
#include <libvalent-media.h>

#include "valent-mpris-device.h"
#include "valent-mpris-plugin.h"
#include "valent-mpris-remote.h"
#include "valent-mpris-utils.h"


struct _ValentMprisPlugin
{
  ValentDevicePlugin  parent_instance;

  ValentMedia        *media;
  gboolean            media_watch : 1;
  GtkWindow          *remote;

  GListModel         *players;
  GHashTable         *transfers;

  GHashTable         *pending;
  unsigned int        flush_id;
};

G_DEFINE_TYPE (ValentMprisPlugin, valent_mpris_plugin, VALENT_TYPE_DEVICE_PLUGIN)

static void valent_mpris_plugin_send_player_info    (ValentMprisPlugin *self,
                                                     ValentMediaPlayer *player,
                                                     gboolean           now_playing,
                                                     gboolean           volume);
static void valent_mpris_plugin_send_player_list    (ValentMprisPlugin *self);


static gpointer
valent_mpris_plugin_get_player (GListModel *players,
                                const char *name)
{
  unsigned int n_players = 0;

  g_assert (G_IS_LIST_MODEL (players));
  g_assert (name != NULL && *name != '\0');

  n_players = g_list_model_get_n_items (players);

  for (unsigned int i = 0; i < n_players; i++)
    {
      g_autoptr (ValentMediaPlayer) player = g_list_model_get_item (players, i);

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
    g_debug ("Failed to upload album art: %s", error->message);

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
  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
  transfer = valent_device_transfer_new_for_file (device, packet, real_file);

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

  self->flush_id = 0;

  return G_SOURCE_REMOVE;
}

static void
on_player_changed (ValentMedia       *media,
                   ValentMediaPlayer *player,
                   ValentMprisPlugin *self)
{
  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  g_hash_table_add (self->pending, player);

  if (self->flush_id == 0)
    self->flush_id = g_idle_add (valent_mpris_plugin_flush, self);
}

static void
on_player_seeked (ValentMedia       *media,
                  ValentMediaPlayer *player,
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
  json_builder_add_int_value (builder, position * 1000L);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
on_players_changed (ValentMedia       *media,
                    unsigned int       position,
                    unsigned int       removed,
                    unsigned int       added,
                    ValentMprisPlugin *self)
{
  if (removed > 0)
    g_hash_table_remove_all (self->pending);

  valent_mpris_plugin_send_player_list (self);
}

static void
valent_mpris_plugin_handle_action (ValentMprisPlugin *self,
                                   ValentMediaPlayer *player,
                                   const char        *action)
{
  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));
  g_assert (action && *action);

  if (strcmp (action, "Next") == 0)
    valent_media_player_next (player);

  else if (strcmp (action, "Pause") == 0)
    valent_media_player_pause (player);

  else if (strcmp (action, "Play") == 0)
    valent_media_player_play (player);

  else if (strcmp (action, "PlayPause") == 0)
    valent_media_player_play_pause (player);

  else if (strcmp (action, "Previous") == 0)
    valent_media_player_previous (player);

  else if (strcmp (action, "Stop") == 0)
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
  gint64 offset_us;
  gint64 position;
  gboolean request_now_playing;
  gboolean request_volume;
  const char *loop_status;
  ValentMediaRepeat repeat;
  gboolean shuffle;
  gint64 volume;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  /* Start by checking for a player */
  if (valent_packet_get_string (packet, "player", &name))
    player = valent_mpris_plugin_get_player (G_LIST_MODEL (self->media), name);

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

  /* A player command */
  if (valent_packet_get_string (packet, "action", &action))
    valent_mpris_plugin_handle_action (self, player, action);

  /* A request to change the relative position (microseconds to seconds) */
  if (valent_packet_get_int (packet, "Seek", &offset_us))
    valent_media_player_seek (player, offset_us / G_TIME_SPAN_SECOND);

  /* A request to change the absolute position (milliseconds to seconds) */
  if (valent_packet_get_int (packet, "SetPosition", &position))
    valent_media_player_set_position (player, position / 1000L);

  /* A request to change the loop status */
  if (valent_packet_get_string (packet, "setLoopStatus", &loop_status))
    {
      repeat = valent_mpris_repeat_from_string (loop_status);
      valent_media_player_set_repeat (player, repeat);
    }

  /* A request to change the shuffle mode */
  if (valent_packet_get_boolean (packet, "setShuffle", &shuffle))
    valent_media_player_set_shuffle (player, shuffle);

  /* A request to change the player volume */
  if (valent_packet_get_int (packet, "setVolume", &volume))
    valent_media_player_set_volume (player, volume / 100.0);

  /* An album art request */
  if (valent_packet_get_string (packet, "albumArtUrl", &url))
    valent_mpris_plugin_send_album_art (self, player, url);
}

static void
valent_mpris_plugin_send_player_info (ValentMprisPlugin *self,
                                      ValentMediaPlayer *player,
                                      gboolean           request_now_playing,
                                      gboolean           request_volume)
{
  const char *name;
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) response = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  /* Start the packet */
  valent_packet_init (&builder, "kdeconnect.mpris");

  name = valent_media_player_get_name (player);
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
      g_autofree char *now_playing = NULL;

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
      json_builder_add_int_value (builder, position * 1000L);

      /* Track Metadata
       *
       * See: https://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata/
       */
      if ((metadata = valent_media_player_get_metadata (player)) != NULL)
        {
          g_autofree const char **artists = NULL;
          gint64 length_us;
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

      /*
       * A composite string only used by kdeconnect-android
       */
      if (artist != NULL && title != NULL)
        now_playing = g_strdup_printf ("%s - %s", artist, title);
      else if (artist != NULL)
        now_playing = g_strdup (artist);
      else if (title != NULL)
        now_playing = g_strdup (title);
      else
        now_playing = g_strdup (_("Unknown"));

      json_builder_set_member_name (builder, "nowPlaying");
      json_builder_add_string_value (builder, now_playing);
    }

  /* Volume Level */
  if (request_volume)
    {
      gint64 level;

      level = floor (valent_media_player_get_volume (player) * 100);
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
  unsigned int n_players = 0;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  valent_packet_init (&builder, "kdeconnect.mpris");

  /* Player List */
  json_builder_set_member_name (builder, "playerList");
  json_builder_begin_array (builder);

  n_players = g_list_model_get_n_items (G_LIST_MODEL (self->media));

  for (unsigned int i = 0; i < n_players; i++)
    {
      g_autoptr (ValentMediaPlayer) player = NULL;
      const char *name;

      player = g_list_model_get_item (G_LIST_MODEL (self->media), i);
      name = valent_media_player_get_name (player);

      if (name != NULL)
        json_builder_add_string_value (builder, name);
    }

  json_builder_end_array (builder);

  /* Album Art */
  json_builder_set_member_name (builder, "supportAlbumArtPayload");
  json_builder_add_boolean_value (builder, TRUE);

  packet = valent_packet_end (&builder);
  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_mpris_plugin_watch_media (ValentMprisPlugin *self,
                                 gboolean           connect)
{
  if (connect == self->media_watch)
    return;

  self->media = valent_media_get_default ();

  if (connect)
    {
      g_signal_connect_object (self->media,
                               "items-changed",
                               G_CALLBACK (on_players_changed),
                               self, 0);
      g_signal_connect_object (self->media,
                               "player-changed",
                               G_CALLBACK (on_player_changed),
                               self, 0);
      g_signal_connect_object (self->media,
                               "player-seeked",
                               G_CALLBACK (on_player_seeked),
                               self, 0);
      self->media_watch = TRUE;
    }
  else
    {
      g_clear_handle_id (&self->flush_id, g_source_remove);
      g_signal_handlers_disconnect_by_data (self->media, self);
      self->media_watch = FALSE;
    }
}

/*
 * Remote Players
 */
static void
valent_mpris_plugin_request_player_list (ValentMprisPlugin *self)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "requestPlayerList");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
receive_art_cb (ValentTransfer    *transfer,
                GAsyncResult      *result,
                ValentMprisPlugin *self)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;
  ValentMprisDevice *player = NULL;
  const char *name;

  if (!valent_transfer_execute_finish (transfer, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  g_object_get (transfer,
                "file",   &file,
                "packet", &packet,
                NULL);

  if (valent_packet_get_string (packet, "player", &name) &&
      (player = valent_mpris_plugin_get_player (self->players, name)) != NULL)
    valent_mpris_device_update_art (player, file);
}

static void
valent_mpris_plugin_receive_album_art (ValentMprisPlugin *self,
                                       JsonNode          *packet)
{
  ValentDevice *device;
  g_autoptr (ValentData) data = NULL;
  const char *url;
  g_autofree char *filename = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (ValentTransfer) transfer = NULL;

  if (!valent_packet_get_string (packet, "albumArtUrl", &url))
    {
      g_warning ("%s(): expected \"albumArtUrl\" field holding a string",
                 G_STRFUNC);
      return;
    }

  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
  data = valent_device_ref_data (device);
  filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
  file = valent_data_create_cache_file (data, filename);

  transfer = valent_device_transfer_new_for_file (device, packet, file);
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)receive_art_cb,
                           self);
}

static void
valent_mpris_plugin_request_update (ValentMprisPlugin *self,
                                    const char        *player)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, player);
  json_builder_set_member_name (builder, "requestNowPlaying");
  json_builder_add_boolean_value (builder, TRUE);
  json_builder_set_member_name (builder, "requestVolume");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_end (&builder);

  valent_device_plugin_queue_packet (VALENT_DEVICE_PLUGIN (self), packet);
}

static void
valent_mpris_plugin_handle_player_list (ValentMprisPlugin *self,
                                        JsonArray         *player_list)
{
  unsigned int n_remote, n_local, n_extant = 0;
  g_autofree const char **remote_names = NULL;
  g_autofree const char **local_names = NULL;
  ValentDevice *device = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (player_list != NULL);

#ifndef __clang_analyzer__
  /* Collect the remote player names */
  n_remote = json_array_get_length (player_list);
  remote_names = g_new0 (const char *, n_remote + 1);

  for (unsigned int i = 0; i < n_remote; i++)
    remote_names[i] = json_array_get_string_element (player_list, i);

  /* Remove old players */
  n_local = g_list_model_get_n_items (self->players);
  local_names = g_new0 (const char *, n_local + 1);

  for (unsigned int i = n_local; i-- > 0;)
    {
      g_autoptr (ValentMediaPlayer) export = NULL;
      const char *name = NULL;

      export = g_list_model_get_item (self->players, i);
      name = valent_media_player_get_name (export);

      if (g_strv_contains (remote_names, name))
        {
          local_names[n_extant++] = name;
          continue;
        }

      valent_media_unexport_player (self->media, export);
      g_list_store_remove (G_LIST_STORE (self->players), i);
    }

  /* Add new players */
  device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));

  for (unsigned int i = 0; remote_names[i] != NULL; i++)
    {
      g_autoptr (ValentMprisDevice) player = NULL;
      const char *name = remote_names[i];

      if (g_strv_contains (local_names, name))
        continue;

      player = valent_mpris_device_new (device);
      valent_mpris_device_update_name (player, name);
      g_list_store_append (G_LIST_STORE (self->players), player);

      valent_media_export_player (self->media, VALENT_MEDIA_PLAYER (player));
      valent_mpris_plugin_request_update (self, name);
    }
#endif /* __clang_analyzer__ */
}

static void
valent_mpris_plugin_handle_player_update (ValentMprisPlugin *self,
                                          JsonNode          *packet)
{
  ValentMprisDevice *player;
  const char *name;

  /* Get the remote */
  if (!valent_packet_get_string (packet, "player", &name) ||
      (player = valent_mpris_plugin_get_player (self->players, name)) == NULL)
    {
      valent_mpris_plugin_request_player_list (self);
      return;
    }

  if (valent_packet_check_field (packet, "transferringAlbumArt"))
    {
      valent_mpris_plugin_receive_album_art (self, packet);
      return;
    }

  valent_mpris_device_handle_packet (player, packet);
}

static void
valent_mpris_plugin_handle_mpris (ValentMprisPlugin *self,
                                  JsonNode          *packet)
{
  JsonArray *player_list;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  if (valent_packet_get_array (packet, "playerList", &player_list))
    valent_mpris_plugin_handle_player_list (self, player_list);

  else if (valent_packet_get_string (packet, "player", NULL))
    valent_mpris_plugin_handle_player_update (self, packet);
}

/*
 * GActions
 */
static void
mpris_remote_action (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (user_data);

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  /* g_assert (gtk_is_initialized ()); */

  if (self->remote == NULL)
    {
      ValentDevice *device;

      device = valent_device_plugin_get_device (VALENT_DEVICE_PLUGIN (self));
      self->remote = g_object_new (VALENT_TYPE_MPRIS_REMOTE,
                                   "device",  device,
                                   "players", self->players,
                                   NULL);
      g_object_add_weak_pointer (G_OBJECT (self->remote),
                                 (gpointer) &self->remote);
    }

  gtk_window_present (self->remote);
}


static const GActionEntry actions[] = {
    {"remote", mpris_remote_action,  NULL, NULL, NULL}
};

static const ValentMenuEntry items[] = {
    {N_("Media Remote"), "device.mpris.remote", "valent-mpris-plugin"}
};

/*
 * ValentDevicePlugin
 */
static void
valent_mpris_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);

  self->media = valent_media_get_default ();

  g_action_map_add_action_entries (G_ACTION_MAP (plugin),
                                   actions,
                                   G_N_ELEMENTS (actions),
                                   plugin);
  valent_device_plugin_add_menu_entries (plugin,
                                         items,
                                         G_N_ELEMENTS (items));
}

static void
valent_mpris_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);

  /* Destroy the presentation remote if necessary */
  g_clear_pointer (&self->remote, gtk_window_destroy);

  valent_device_plugin_remove_menu_entries (plugin,
                                            items,
                                            G_N_ELEMENTS (items));

  valent_mpris_plugin_watch_media (self, FALSE);
  self->media = NULL;
}

static void
valent_mpris_plugin_update_state (ValentDevicePlugin *plugin,
                                  ValentDeviceState   state)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);
  gboolean available;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  available = (state & VALENT_DEVICE_STATE_CONNECTED) != 0 &&
              (state & VALENT_DEVICE_STATE_PAIRED) != 0;

  if (available)
    {
      valent_mpris_plugin_watch_media (self, TRUE);
      valent_mpris_plugin_request_player_list (self);
      valent_mpris_plugin_send_player_list (self);
    }
  else
    {
      valent_mpris_plugin_watch_media (self, FALSE);
      g_list_store_remove_all (G_LIST_STORE (self->players));
    }
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

  if (strcmp (type, "kdeconnect.mpris") == 0)
    valent_mpris_plugin_handle_mpris (self, packet);

  else if (strcmp (type, "kdeconnect.mpris.request") == 0)
    valent_mpris_plugin_handle_mpris_request (self, packet);

  else
    g_assert_not_reached ();
}

/*
 * GObject
 */
static void
valent_mpris_plugin_finalize (GObject *object)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (object);

  g_clear_pointer (&self->pending, g_hash_table_unref);
  g_clear_object (&self->players);
  g_clear_pointer (&self->transfers, g_hash_table_unref);

  G_OBJECT_CLASS (valent_mpris_plugin_parent_class)->finalize (object);
}

static void
valent_mpris_plugin_class_init (ValentMprisPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentDevicePluginClass *plugin_class = VALENT_DEVICE_PLUGIN_CLASS (klass);

  object_class->finalize = valent_mpris_plugin_finalize;

  plugin_class->enable = valent_mpris_plugin_enable;
  plugin_class->disable = valent_mpris_plugin_disable;
  plugin_class->handle_packet = valent_mpris_plugin_handle_packet;
  plugin_class->update_state = valent_mpris_plugin_update_state;
}

static void
valent_mpris_plugin_init (ValentMprisPlugin *self)
{
  self->players = G_LIST_MODEL (g_list_store_new (VALENT_TYPE_MEDIA_PLAYER));
  self->transfers = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           g_object_unref);
  self->pending = g_hash_table_new (NULL, NULL);
}

