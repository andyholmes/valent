// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mpris-plugin"

#include "config.h"

#include <math.h>
#include <glib/gi18n.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-media.h>

#include "valent-mpris-plugin.h"
#include "valent-mpris-remote.h"


struct _ValentMprisPlugin
{
  PeasExtensionBase  parent_instance;

  ValentDevice      *device;
  ValentMedia       *media;

  gboolean           media_watch : 1;

  GHashTable        *remotes;
  GHashTable        *artwork_transfers;
};

static void valent_device_plugin_iface_init (ValentDevicePluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMprisPlugin, valent_mpris_plugin, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_DEVICE_PLUGIN, valent_device_plugin_iface_init))

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static void valent_mpris_plugin_handle_action       (ValentMprisPlugin *self,
                                                     ValentMediaPlayer *player,
                                                     const char        *action);
static void valent_mpris_plugin_request_player_list (ValentMprisPlugin *self);
static void valent_mpris_plugin_request_update      (ValentMprisPlugin *self,
                                                     const char        *name);
static void valent_mpris_plugin_send_album_art      (ValentMprisPlugin *self,
                                                     ValentMediaPlayer *player,
                                                     const char        *uri);
static void valent_mpris_plugin_send_player_info    (ValentMprisPlugin *self,
                                                     ValentMediaPlayer *player,
                                                     gboolean           now_playing,
                                                     gboolean           volume);
static void valent_mpris_plugin_send_player_list    (ValentMprisPlugin *self);


/*
 * Local Players
 */
static void
send_album_art_cb (ValentTransfer    *transfer,
                   GAsyncResult      *result,
                   ValentMprisPlugin *self)
{
  g_autoptr (GError) error = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  if (!valent_transfer_execute_finish (transfer, result, &error))
    g_debug ("Failed to upload album art: %s", error->message);

  g_hash_table_remove (self->artwork_transfers, valent_transfer_get_id (transfer));
}

static void
valent_mpris_plugin_send_album_art (ValentMprisPlugin *self,
                                    ValentMediaPlayer *player,
                                    const char        *url)
{
  g_autoptr (GVariant) metadata = NULL;
  const char *art_url;
  g_autoptr (GFile) art_file = NULL;
  g_autoptr (GFile) req_file = NULL;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (ValentTransfer) transfer = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  /* Reject concurrent requests */
  if (g_hash_table_contains (self->artwork_transfers, url))
    return;

  /* Check player and URL are safe */
  if ((metadata = valent_media_player_get_metadata (player)) == NULL ||
      !g_variant_lookup (metadata, "mpris:artUrl", "&s", &art_url))
    {
      g_warning ("Album art request \"%s\" for track without album art", url);
      return;
    }

  /* Compare normalized URLs */
  req_file = g_file_new_for_uri (url);
  art_file = g_file_new_for_uri (art_url);

  if (!g_file_equal (req_file, art_file))
    {
      g_warning ("Album art request \"%s\" doesn't match current track \"%s\"",
                 url, art_url);
      return;
    }

  /* Build the payload packet */
  builder = valent_packet_start ("kdeconnect.mpris");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, valent_media_player_get_name (player));
  json_builder_set_member_name (builder, "albumArtUrl");
  json_builder_add_string_value (builder, art_url);
  json_builder_set_member_name (builder, "transferringAlbumArt");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  /* Start the transfer */
  transfer = valent_transfer_new (self->device);
  valent_transfer_set_id (transfer, url);
  valent_transfer_add_file (transfer, packet, art_file);

  g_hash_table_add (self->artwork_transfers,
                    (char *)valent_transfer_get_id (transfer));

  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)send_album_art_cb,
                           self);
}

static void
on_player_changed (ValentMedia       *media,
                   ValentMediaPlayer *player,
                   ValentMprisPlugin *self)
{
  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  valent_mpris_plugin_send_player_info (self, player, TRUE, TRUE);
}

static void
on_player_seeked (ValentMedia       *media,
                  ValentMediaPlayer *player,
                  gint64             position,
                  ValentMprisPlugin *self)
{
  const char *name;
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  name = valent_media_player_get_name (player);

  builder = valent_packet_start ("kdeconnect.mpris");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, name);
  json_builder_set_member_name (builder, "pos");
  json_builder_add_int_value (builder, floor (position / 1000));
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
on_players_changed (ValentMedia       *media,
                    ValentMediaPlayer *player,
                    ValentMprisPlugin *self)
{
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

  if (g_str_equal (action, "Next"))
    valent_media_player_next (player);

  else if (g_str_equal (action, "Pause"))
    valent_media_player_pause (player);

  else if (g_str_equal (action, "Play"))
    valent_media_player_play (player);

  else if (g_str_equal (action, "PlayPause"))
    valent_media_player_play_pause (player);

  else if (g_str_equal (action, "Previous"))
    valent_media_player_previous (player);

  else if (g_str_equal (action, "Stop"))
    valent_media_player_stop (player);

  else
    g_warning ("[%s]: Unknown action: %s", G_STRFUNC, action);
}

static void
valent_mpris_plugin_handle_mpris_request (ValentMprisPlugin *self,
                                          JsonNode          *packet)
{
  JsonObject *body;
  ValentMediaPlayer *player = NULL;
  const char *name;
  const char *action;
  const char *url;
  gint64 offset;
  gboolean now_playing;
  gboolean volume;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  body = valent_packet_get_body (packet);

  /* Start by checking for a player */
  if ((name = valent_packet_check_string (body, "player")) != NULL)
    player = valent_media_get_player_by_name (self->media, name);

  if (player == NULL || valent_packet_check_boolean (body, "requestPlayerList"))
    {
      valent_mpris_plugin_send_player_list (self);
      return;
    }

  /* A request for a player's status */
  now_playing = valent_packet_check_boolean (body, "requestNowPlaying");
  volume = valent_packet_check_boolean (body, "requestVolume");

  if (now_playing || volume)
    valent_mpris_plugin_send_player_info (self, player, now_playing, volume);

  /* A player command */
  if ((action = valent_packet_check_string (body, "action")) != NULL)
    valent_mpris_plugin_handle_action (self, player, action);

  /* A request to change the relative position */
  if ((offset = valent_packet_check_int (body, "Seek")) != 0)
    valent_media_player_seek (player, offset * 1000);

  /* A request to change the absolute position */
  if (json_object_has_member (body, "SetPosition"))
    {
      gint64 offset;
      gint64 position;

      position = json_object_get_int_member (body, "SetPosition") * 1000;
      offset = position - valent_media_player_get_position (player);
      valent_media_player_seek (player, offset);
    }

  /* A request to change the player volume */
  if (json_object_has_member (body, "setVolume"))
    {
      gint64 volume;

      volume = valent_packet_check_int (body, "setVolume");
      valent_media_player_set_volume (player, volume / 100.0);
    }

  /* An album art request */
  if ((url = valent_packet_check_string (body, "albumArtUrl")) != NULL)
    valent_mpris_plugin_send_album_art (self, player, url);
}

static void
valent_mpris_plugin_send_player_info (ValentMprisPlugin *self,
                                      ValentMediaPlayer *player,
                                      gboolean           now_playing,
                                      gboolean           volume)
{
  const char *name;
  JsonBuilder *builder;
  g_autoptr (JsonNode) response = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  /* Start the packet */
  builder = valent_packet_start ("kdeconnect.mpris");

  name = valent_media_player_get_name (player);
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, name);

  /* Player State & Metadata */
  if (now_playing)
    {
      ValentMediaActions flags;
      gboolean is_playing;
      gint64 position;

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

      is_playing = valent_media_player_is_playing (player);
      json_builder_set_member_name (builder, "isPlaying");
      json_builder_add_boolean_value (builder, is_playing);

      position = valent_media_player_get_position (player);
      json_builder_set_member_name (builder, "pos");
      json_builder_add_int_value (builder, position / 1000);

      /* Track Metadata
       *
       * See: https://www.freedesktop.org/wiki/Specifications/mpris-spec/metadata/
       */
      if ((metadata = valent_media_player_get_metadata (player)) != NULL)
        {
          g_autofree const char **artists = NULL;
          gint64 length;
          const char *art_url;
          const char *album;

          if (g_variant_lookup (metadata, "xesam:artist", "^a&s", &artists) &&
              g_strv_length ((char **)artists) > 0 &&
              g_utf8_strlen (artists[0], -1) > 0)
            {
              artist = g_strjoinv (", ", (char **)artists);
              json_builder_set_member_name (builder, "artist");
              json_builder_add_string_value (builder, artist);
            }

          if (g_variant_lookup (metadata, "xesam:title", "&s", &title) &&
              g_utf8_strlen (title, -1) > 0)
            {
              json_builder_set_member_name (builder, "title");
              json_builder_add_string_value (builder, title);
            }

          if (g_variant_lookup (metadata, "xesam:album", "&s", &album) &&
              g_utf8_strlen (album, -1) > 0)
            {
              json_builder_set_member_name (builder, "album");
              json_builder_add_string_value (builder, album);
            }

          if (g_variant_lookup (metadata, "mpris:length", "x", &length))
            {
              json_builder_set_member_name (builder, "length");
              json_builder_add_int_value (builder, length / 1000);
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
  if (volume)
    {
      double level;

      level = floor (valent_media_player_get_volume (player) * 100);
      json_builder_set_member_name (builder, "volume");
      json_builder_add_double_value (builder, level);
    }

  /* Send Response */
  response = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, response);
}

static void
valent_mpris_plugin_send_player_list (ValentMprisPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GPtrArray) players = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.mpris");

  /* Player List */
  json_builder_set_member_name (builder, "playerList");
  json_builder_begin_array (builder);

  players = valent_media_get_players (self->media);

  for (unsigned int i = 0; i < players->len; i++)
    {
      ValentMediaPlayer *player;
      const char *name;

      player = g_ptr_array_index (players, i);

      if ((name = valent_media_player_get_name (player)) != NULL)
        json_builder_add_string_value (builder, name);
    }

  json_builder_end_array (builder);

  /* Album Art */
  json_builder_set_member_name (builder, "supportAlbumArtPayload");
  json_builder_add_boolean_value (builder, TRUE);

  packet = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, packet);
}

static void
watch_media (ValentMprisPlugin *self,
             gboolean           connect)
{
  if (connect == self->media_watch)
    return;

  self->media = valent_media_get_default ();

  if (connect)
    {
      g_signal_connect (self->media,
                        "player-added",
                        G_CALLBACK (on_players_changed),
                        self);

      g_signal_connect (self->media,
                        "player-removed",
                        G_CALLBACK (on_players_changed),
                        self);

      g_signal_connect (self->media,
                        "player-changed",
                        G_CALLBACK (on_player_changed),
                        self);

      g_signal_connect (self->media,
                        "player-seeked",
                        G_CALLBACK (on_player_seeked),
                        self);

      self->media_watch = TRUE;
    }
  else
    {
      g_signal_handlers_disconnect_by_data (self->media, self);
      self->media_watch = FALSE;
    }
}

/*
 * Remote Players
 */
static void
remote_export_cb (ValentMprisRemote *remote,
                  GAsyncResult      *result,
                  ValentMprisPlugin *self)
{
  g_autoptr (GError) error = NULL;
  const char *name;

  name = valent_media_player_get_name (VALENT_MEDIA_PLAYER (remote));

  if (valent_mpris_remote_export_finish (remote, result, &error))
    valent_mpris_plugin_request_update (self, name);
  else
    g_warning ("Exporting %s: %s", name, error->message);
}

static void
on_remote_method (ValentMprisRemote *remote,
                  const char        *method_name,
                  GVariant          *args,
                  ValentMprisPlugin *self)
{
  ValentMediaPlayer *player = VALENT_MEDIA_PLAYER (remote);
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));

  builder = valent_packet_start ("kdeconnect.mpris.request");

  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, valent_media_player_get_name (player));

  if (g_strcmp0 (method_name, "PlayPause") == 0 ||
      g_strcmp0 (method_name, "Play") == 0 ||
      g_strcmp0 (method_name, "Pause") == 0 ||
      g_strcmp0 (method_name, "Stop") == 0 ||
      g_strcmp0 (method_name, "Next") == 0 ||
      g_strcmp0 (method_name, "Previous") == 0)
    {
      json_builder_set_member_name (builder, "action");
      json_builder_add_string_value (builder, method_name);
    }

  // TODO: kdeconnect-android doesn't support `Seek` and instead expects a
  //       `SetPosition` command...
  else if (g_strcmp0 (method_name, "Seek") == 0)
    {
      gint64 offset;

      g_variant_get (args, "(x)", &offset);
      json_builder_set_member_name (builder, "Seek");
      json_builder_add_int_value (builder, floor (offset / 1000));
    }

  // TODO: test kdeconnect-android response to this
  else if (g_strcmp0 (method_name, "SetPosition") == 0)
    {
      const char *track_id;
      gint64 position;

      g_variant_get (args, "(&ox)", &track_id, &position);
      json_builder_set_member_name (builder, "SetPosition");
      json_builder_add_int_value (builder, floor (position / 1000));
    }
  else
    {
      g_object_unref (builder);
      return;
    }

  packet = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, packet);
}

static void
on_remote_set_property (ValentMprisRemote *remote,
                        const char        *property_name,
                        GVariant          *value,
                        ValentMprisPlugin *self)
{
  ValentMediaPlayer *player = VALENT_MEDIA_PLAYER (remote);
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.mpris.request");

  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, valent_media_player_get_name (player));

  if (g_strcmp0 (property_name, "Volume") == 0)
    {
      double volume;

      volume = g_variant_get_double (value);

      json_builder_set_member_name (builder, "setVolume");
      json_builder_add_int_value (builder, floor (volume * 100));
    }
  else
    {
      g_object_unref (builder);
      return;
    }

  packet = valent_packet_finish (builder);
  valent_device_queue_packet (self->device, packet);
}

static void
valent_mpris_plugin_request_player_list (ValentMprisPlugin *self)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "requestPlayerList");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

typedef struct
{
  ValentMprisPlugin *self;
  JsonNode          *packet;
  GFile             *file;
} AlbumArtOperation;

static void
receive_art_cb (ValentTransfer *transfer,
                GAsyncResult   *result,
                gpointer        user_data)
{
  g_autofree AlbumArtOperation *op = user_data;
  g_autoptr (ValentMprisPlugin) self = g_steal_pointer (&op->self);
  g_autoptr (JsonNode) packet = g_steal_pointer (&op->packet);
  g_autoptr (GFile) file = g_steal_pointer (&op->file);
  g_autoptr (GError) error = NULL;
  JsonObject *body;
  const char *player;
  ValentMprisRemote *remote;

  if (!valent_transfer_execute_finish (transfer, result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s: %s", G_STRFUNC, error->message);

      return;
    }

  body = valent_packet_get_body (packet);

  if ((player = valent_packet_check_string (body, "player")) != NULL &&
      (remote = g_hash_table_lookup (self->remotes, player)) != NULL)
    valent_mpris_remote_update_art (remote, file);
}

static void
valent_mpris_plugin_receive_album_art (ValentMprisPlugin *self,
                                       JsonNode          *packet)
{
  ValentData *data;
  const char *url;
  AlbumArtOperation *op;
  g_autofree char *filename = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (ValentTransfer) transfer = NULL;
  JsonObject *body;

  body = valent_packet_get_body (packet);

  if ((url = valent_packet_check_string (body, "albumArtUrl")) == NULL)
    {
      g_warning ("%s: Invalid \"albumArtUrl\" field", G_STRFUNC);
      return;
    }

  data = valent_device_get_data (self->device);
  filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
  file = valent_data_new_cache_file (data, filename);

  op = g_new0 (AlbumArtOperation, 1);
  op->self = g_object_ref (self);
  op->packet = json_node_ref (packet);
  op->file = g_object_ref (file);

  transfer = valent_transfer_new (self->device);
  valent_transfer_add_file (transfer, packet, file);
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)receive_art_cb,
                           op);
}

static void
valent_mpris_plugin_request_album_art (ValentMprisPlugin *self,
                                       const char        *player,
                                       const char        *url,
                                       GVariantDict      *metadata)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  ValentData *data;
  g_autoptr (GFile) file = NULL;
  g_autofree char *filename = NULL;

  data = valent_device_get_data (self->device);
  filename = g_compute_checksum_for_string (G_CHECKSUM_MD5, url, -1);
  file = valent_data_new_cache_file (data, filename);

  /* If the album art has been cached, update the metadata dictionary */
  if (g_file_query_exists (file, NULL))
    {
      g_autofree char *art_url = NULL;

      art_url = g_file_get_uri (file);
      g_variant_dict_insert (metadata, "mpris:artUrl", "s", art_url);

      return;
    }

  /* Request the album art payload */
  builder = valent_packet_start ("kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, player);
  json_builder_set_member_name (builder, "albumArtUrl");
  json_builder_add_string_value (builder, url);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mpris_plugin_request_update (ValentMprisPlugin *self,
                                    const char        *player)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;

  builder = valent_packet_start ("kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, player);
  json_builder_set_member_name (builder, "requestNowPlaying");
  json_builder_add_boolean_value (builder, TRUE);
  json_builder_set_member_name (builder, "requestVolume");
  json_builder_add_boolean_value (builder, TRUE);
  packet = valent_packet_finish (builder);

  valent_device_queue_packet (self->device, packet);
}

static void
valent_mpris_plugin_handle_player_list (ValentMprisPlugin *self,
                                        JsonArray         *player_list)
{
  GHashTableIter iter;
  unsigned int n_players;
  gpointer key;
  g_autofree const char **names = NULL;

  n_players = json_array_get_length (player_list);
  names = g_new (const char *, n_players + 1);

  for (unsigned int i = 0; i < n_players; i++)
    names[i] = json_array_get_string_element (player_list, i);
  names[n_players] = NULL;

  /* Remove old players */
  g_hash_table_iter_init (&iter, self->remotes);

  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      if (!g_strv_contains (names, key))
        g_hash_table_iter_remove (&iter);
    }

  /* Add new players */
  for (unsigned int i = 0; names[i]; i++)
    {
      ValentMprisRemote *remote;

      if (g_hash_table_contains (self->remotes, names[i]))
        continue;

      remote = valent_mpris_remote_new ();
      valent_mpris_remote_set_name (remote, names[i]);
      g_hash_table_insert (self->remotes, g_strdup (names[i]), remote);

      g_signal_connect (remote,
                        "player-method",
                        G_CALLBACK (on_remote_method),
                        self);
      g_signal_connect (remote,
                        "set-property",
                        G_CALLBACK (on_remote_set_property),
                        self);

      valent_mpris_remote_export (remote,
                                  NULL,
                                  (GAsyncReadyCallback)remote_export_cb,
                                  self);
    }
}

static void
valent_mpris_plugin_handle_player_update (ValentMprisPlugin *self,
                                          JsonNode          *packet,
                                          JsonObject        *update)
{
  ValentMprisRemote *remote;
  const char *name;
  const char *url;
  ValentMediaActions flags = VALENT_MEDIA_ACTION_NONE;
  GVariantDict metadata;
  const char *artist, *title, *album;
  gint64 length, position;
  double volume;
  gboolean is_playing;

  /* Get the remote */
  name = json_object_get_string_member (update, "player");

  if G_UNLIKELY ((remote = g_hash_table_lookup (self->remotes, name)) == NULL)
    {
      valent_mpris_plugin_request_player_list (self);
      return;
    }

  if (valent_packet_check_boolean (update, "transferringAlbumArt"))
    {
      valent_mpris_plugin_receive_album_art (self, packet);
      return;
    }

  /* Available actions */
  if (valent_packet_check_boolean (update, "canGoNext"))
    flags |= VALENT_MEDIA_ACTION_NEXT;

  if (valent_packet_check_boolean (update, "canGoPrevious"))
    flags |= VALENT_MEDIA_ACTION_PREVIOUS;

  if (valent_packet_check_boolean (update, "canPause"))
    flags |= VALENT_MEDIA_ACTION_PAUSE;

  if (valent_packet_check_boolean (update, "canPlay"))
    flags |= VALENT_MEDIA_ACTION_PLAY;

  if (valent_packet_check_boolean (update, "canSeek"))
    flags |= VALENT_MEDIA_ACTION_SEEK;

  /* Metadata */
  g_variant_dict_init (&metadata, NULL);

  if ((artist = valent_packet_check_string (update, "artist")) != NULL)
    {
      g_auto (GStrv) artists = NULL;
      GVariant *value;

      artists = g_strsplit (artist, ",", -1);
      value = g_variant_new_strv ((const char * const *)artists, -1);
      g_variant_dict_insert_value (&metadata, "xesam:artist", value);
    }

  if ((title = valent_packet_check_string (update, "title")) != NULL)
    g_variant_dict_insert (&metadata, "xesam:title", "s", title);

  if ((album = valent_packet_check_string (update, "album")) != NULL)
    g_variant_dict_insert (&metadata, "xesam:album", "s", album);

  if ((length = valent_packet_check_int (update, "length")) != 0)
    g_variant_dict_insert (&metadata, "mpris:length", "x", length);

  if ((url = valent_packet_check_string (update, "albumArtUrl")) != NULL)
    valent_mpris_plugin_request_album_art (self, name, url, &metadata);

  /* Playback Status */
  is_playing = valent_packet_check_boolean (update, "isPlaying");
  position = valent_packet_check_int (update, "pos");
  volume = json_object_get_double_member_with_default (update, "volume", 100.0) / 100;

  valent_mpris_remote_update_player (remote,
                                     flags,
                                     g_variant_dict_end (&metadata),
                                     is_playing ? "Playing" : "Paused",
                                     position,
                                     volume);
}

static void
valent_mpris_plugin_handle_mpris (ValentMprisPlugin *self,
                                  JsonNode          *packet)
{
  JsonObject *body;

  g_assert (VALENT_IS_MPRIS_PLUGIN (self));
  g_assert (VALENT_IS_PACKET (packet));

  body = valent_packet_get_body (packet);

  if (json_object_has_member (body, "playerList"))
    {
      JsonArray *player_list;

      player_list = json_object_get_array_member (body, "playerList");
      valent_mpris_plugin_handle_player_list (self, player_list);
    }

  else if (json_object_has_member (body, "player"))
    {
      valent_mpris_plugin_handle_player_update (self, packet, body);
    }
}

/*
 * ValentDevicePlugin
 */
static void
valent_mpris_plugin_enable (ValentDevicePlugin *plugin)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);

  self->media = valent_media_get_default ();
}

static void
valent_mpris_plugin_disable (ValentDevicePlugin *plugin)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);

  self->media = NULL;
}

static void
valent_mpris_plugin_update_state (ValentDevicePlugin *plugin)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (plugin);
  gboolean connected;
  gboolean paired;
  gboolean available;

  connected = valent_device_get_connected (self->device);
  paired = valent_device_get_paired (self->device);
  available = (connected && paired);

  /* Media Players */
  watch_media (self, available);

  if (available)
    {
      valent_mpris_plugin_request_player_list (self);
      valent_mpris_plugin_send_player_list (self);
    }
  else
    {
      g_hash_table_remove_all (self->remotes);
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

  if (g_strcmp0 (type, "kdeconnect.mpris") == 0)
    valent_mpris_plugin_handle_mpris (self, packet);

  else if (g_strcmp0 (type, "kdeconnect.mpris.request") == 0)
    valent_mpris_plugin_handle_mpris_request (self, packet);

  else
    g_assert_not_reached ();
}

static void
valent_device_plugin_iface_init (ValentDevicePluginInterface *iface)
{
  iface->enable = valent_mpris_plugin_enable;
  iface->disable = valent_mpris_plugin_disable;
  iface->handle_packet = valent_mpris_plugin_handle_packet;
  iface->update_state = valent_mpris_plugin_update_state;
}

/*
 * GObject
 */
static void
valent_mpris_plugin_finalize (GObject *object)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (object);

  watch_media (self, FALSE);
  g_clear_pointer (&self->artwork_transfers, g_hash_table_unref);
  g_clear_pointer (&self->remotes, g_hash_table_unref);

  G_OBJECT_CLASS (valent_mpris_plugin_parent_class)->finalize (object);
}

static void
valent_mpris_plugin_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (object);

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
valent_mpris_plugin_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentMprisPlugin *self = VALENT_MPRIS_PLUGIN (object);

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
valent_mpris_plugin_class_init (ValentMprisPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_mpris_plugin_finalize;
  object_class->get_property = valent_mpris_plugin_get_property;
  object_class->set_property = valent_mpris_plugin_set_property;

  g_object_class_override_property (object_class, PROP_DEVICE, "device");
}

static void
valent_mpris_plugin_init (ValentMprisPlugin *self)
{
  self->artwork_transfers = g_hash_table_new (g_str_hash, g_str_equal);
  self->remotes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                         g_free, g_object_unref);
}

