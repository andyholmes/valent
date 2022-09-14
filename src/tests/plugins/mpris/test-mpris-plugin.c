// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-media.h>
#include <libvalent-test.h>

#include "valent-mpris-player.h"
#include "valent-mpris-remote.h"


static ValentMPRISPlayer *proxy = NULL;


static void
on_player_added (ValentMedia       *media,
                 ValentMediaPlayer *player,
                 ValentTestFixture *fixture)
{
  if (g_strcmp0 (valent_media_player_get_name (player), "Test Player") == 0)
    valent_test_fixture_quit (fixture);
}

static void
on_player_removed (ValentMedia       *media,
                   ValentMediaPlayer *player,
                   ValentTestFixture *fixture)
{
  if (g_strcmp0 (valent_media_player_get_name (player), "Test Player") == 0)
    valent_test_fixture_quit (fixture);
}

static void
on_remote_method (ValentMprisRemote *remote,
                  const char        *method,
                  GVariant          *args,
                  ValentTestFixture *fixture)
{
  GVariantDict dict;
  static const char * const artist[] = { "Test Artist" };
  ValentMediaActions flags = VALENT_MEDIA_ACTION_NONE;

  /* Fake playback start */
  if (g_strcmp0 (method, "Play") == 0 || g_strcmp0 (method, "Previous") == 0)
    {
      g_variant_dict_init (&dict, NULL);
      g_variant_dict_insert_value (&dict, "xesam:artist", g_variant_new_strv (artist, 1));
      g_variant_dict_insert (&dict, "xesam:title", "s", "Track 1");
      g_variant_dict_insert (&dict, "xesam:album", "s", "Test Album");
      g_variant_dict_insert (&dict, "mpris:length", "x", G_TIME_SPAN_MINUTE * 3);

      flags |= (VALENT_MEDIA_ACTION_NEXT |
                VALENT_MEDIA_ACTION_PAUSE |
                VALENT_MEDIA_ACTION_SEEK);

      valent_mpris_remote_update_full (remote,
                                       flags,
                                       g_variant_dict_end (&dict),
                                       "Playing",
                                       0,
                                       "None",
                                       FALSE,
                                       1.0);
    }

  /* Fake track next */
  else if (g_strcmp0 (method, "Next") == 0)
    {
      g_variant_dict_init (&dict, NULL);
      g_variant_dict_insert_value (&dict, "xesam:artist", g_variant_new_strv (artist, 1));
      g_variant_dict_insert (&dict, "xesam:title", "s", "Track 2");
      g_variant_dict_insert (&dict, "xesam:album", "s", "Test Album");
      g_variant_dict_insert (&dict, "mpris:length", "x", G_TIME_SPAN_MINUTE * 3);

      flags |= (VALENT_MEDIA_ACTION_NEXT |
                VALENT_MEDIA_ACTION_PREVIOUS |
                VALENT_MEDIA_ACTION_PAUSE |
                VALENT_MEDIA_ACTION_SEEK);

      valent_mpris_remote_update_full (remote,
                                       flags,
                                       g_variant_dict_end (&dict),
                                       "Playing",
                                       0,
                                       "None",
                                       FALSE,
                                       1.0);
    }

  /* Fake playback pause */
  else if (g_strcmp0 (method, "Pause") == 0)
    {

      flags |= (VALENT_MEDIA_ACTION_NEXT |
                VALENT_MEDIA_ACTION_PREVIOUS |
                VALENT_MEDIA_ACTION_PLAY |
                VALENT_MEDIA_ACTION_SEEK);

      valent_mpris_remote_update_full (remote,
                                       flags,
                                       NULL,
                                       "Paused",
                                       0,
                                       "None",
                                       FALSE,
                                       1.0);
    }

  /* Fake seek */
  else if (g_strcmp0 (method, "Seek") == 0)
    {
      gint64 offset;

      g_variant_get (args, "(x)", &offset);
      valent_mpris_remote_emit_seeked (remote, offset);
    }

  /* Fake playback stop */
  else if (g_strcmp0 (method, "Stop") == 0)
    {
      g_variant_dict_init (&dict, NULL);

      valent_mpris_remote_update_full (remote,
                                       flags,
                                       g_variant_dict_end (&dict),
                                       "Stopped",
                                       0,
                                       "None",
                                       FALSE,
                                       1.0);
    }
}

static void
on_remote_set_property (ValentMprisRemote *remote,
                        const char        *name,
                        GVariant          *value,
                        ValentTestFixture *fixture)
{
  if (strcmp (name, "LoopStatus") == 0)
    valent_mpris_remote_update_repeat (remote, g_variant_get_string (value, NULL));

  if (strcmp (name, "Shuffle") == 0)
    valent_mpris_remote_update_shuffle (remote, g_variant_get_boolean (value));

  if (strcmp (name, "Volume") == 0)
    valent_mpris_remote_update_volume (remote, g_variant_get_double (value));
}

static void
export_cb (ValentMprisRemote *remote,
           GAsyncResult      *result,
           ValentTestFixture *fixture)
{
  g_autoptr (GError) error = NULL;

  valent_mpris_remote_export_finish (remote, result, &error);
  g_assert_no_error (error);
}

static JsonNode *
create_albumart_request (const char *art_url)
{
  JsonBuilder *builder;

  builder = valent_packet_start ("kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, "Test Player");
  json_builder_set_member_name (builder, "albumArtUrl");
  json_builder_add_string_value (builder, art_url);

  return valent_packet_finish (builder);
}

static void
test_mpris_plugin_handle_request (ValentTestFixture *fixture,
                                  gconstpointer      user_data)
{
  ValentMedia *media;
  g_autoptr (ValentMprisRemote) remote = NULL;
  g_autoptr (GError) error = NULL;
  GVariantDict dict;
  JsonNode *packet;
  JsonArray *player_list;
  const char *player_name;

  media = valent_media_get_default ();
  g_signal_connect (media,
                    "player-added",
                    G_CALLBACK (on_player_added),
                    fixture);
  g_signal_connect (media,
                    "player-removed",
                    G_CALLBACK (on_player_removed),
                    fixture);

  /* Export a mock player that we can use to poke the plugin during testing */
  remote = valent_mpris_remote_new ();
  valent_mpris_remote_set_name (remote, "Test Player");
  valent_mpris_remote_export_full (remote,
                                   "org.mpris.MediaPlayer2.Test",
                                   NULL,
                                   (GAsyncReadyCallback)export_cb,
                                   fixture);
  valent_test_fixture_run (fixture);

  g_signal_connect (remote,
                    "method-call",
                    G_CALLBACK (on_remote_method),
                    fixture);
  g_signal_connect (remote,
                    "set-property",
                    G_CALLBACK (on_remote_set_property),
                    fixture);

  /* Connect and read handshake packets */
  valent_test_fixture_connect (fixture, TRUE);

  /* Expect a request for our players */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_true (packet, "requestPlayerList");
  json_node_unref (packet);

  /* Expect a list of their players, which should include the mock player */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  player_list = json_object_get_array_member (valent_packet_get_body (packet),
                                              "playerList");
  player_name = json_array_get_string_element (player_list, 0);
  g_assert_cmpstr (player_name, ==, "Test Player");
  json_node_unref (packet);

  /* Request player state */
  packet = valent_test_fixture_lookup_packet (fixture, "request-nowplaying");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect quiescent state */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_false (packet, "canPause");
  v_assert_packet_false (packet, "canPlay");
  v_assert_packet_false (packet, "canGoNext");
  v_assert_packet_false (packet, "canGoPrevious");
  v_assert_packet_false (packet, "canSeek");
  v_assert_packet_false (packet, "isPlaying");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "None");
  v_assert_packet_false (packet, "shuffle");
  v_assert_packet_cmpint (packet, "volume", ==, 100);

  v_assert_packet_no_field (packet, "artist");
  v_assert_packet_no_field (packet, "title");
  v_assert_packet_no_field (packet, "album");
  v_assert_packet_no_field (packet, "length");

  json_node_unref (packet);

  /* Request Play */
  packet = valent_test_fixture_lookup_packet (fixture, "request-play");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect play state */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_true (packet, "canPause");
  v_assert_packet_true (packet, "canGoNext");
  v_assert_packet_true (packet, "canSeek");
  v_assert_packet_true (packet, "isPlaying");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "None");
  v_assert_packet_false (packet, "shuffle");

  v_assert_packet_cmpstr (packet, "artist", ==, "Test Artist");
  v_assert_packet_cmpstr (packet, "title", ==, "Track 1");
  v_assert_packet_cmpstr (packet, "album", ==, "Test Album");
  v_assert_packet_cmpint (packet, "length", ==, 180000);
  json_node_unref (packet);

  /* Request Next */
  packet = valent_test_fixture_lookup_packet (fixture, "request-next");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect Track 2 */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_true (packet, "canPause");
  v_assert_packet_true (packet, "canGoNext");
  v_assert_packet_true (packet, "canGoPrevious");
  v_assert_packet_true (packet, "canSeek");
  v_assert_packet_true (packet, "isPlaying");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "None");
  v_assert_packet_false (packet, "shuffle");

  v_assert_packet_cmpstr (packet, "artist", ==, "Test Artist");
  v_assert_packet_cmpstr (packet, "title", ==, "Track 2");
  v_assert_packet_cmpstr (packet, "album", ==, "Test Album");
  v_assert_packet_cmpint (packet, "length", ==, 180000);
  json_node_unref (packet);

  /* Request Previous */
  packet = valent_test_fixture_lookup_packet (fixture, "request-previous");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect Track 1 */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_true (packet, "canPause");
  v_assert_packet_true (packet, "canGoNext");
  v_assert_packet_false (packet, "canGoPrevious");
  v_assert_packet_true (packet, "canSeek");
  v_assert_packet_true (packet, "isPlaying");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "None");
  v_assert_packet_false (packet, "shuffle");

  v_assert_packet_cmpstr (packet, "artist", ==, "Test Artist");
  v_assert_packet_cmpstr (packet, "title", ==, "Track 1");
  v_assert_packet_cmpstr (packet, "album", ==, "Test Album");
  v_assert_packet_cmpint (packet, "length", ==, 180000);
  json_node_unref (packet);

  /* Request Pause */
  packet = valent_test_fixture_lookup_packet (fixture, "request-pause");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect paused state */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_false (packet, "canPause");
  v_assert_packet_true (packet, "canPlay");
  v_assert_packet_true (packet, "canSeek");
  v_assert_packet_false (packet, "isPlaying");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "None");
  v_assert_packet_false (packet, "shuffle");

  v_assert_packet_cmpstr (packet, "artist", ==, "Test Artist");
  v_assert_packet_cmpstr (packet, "title", ==, "Track 1");
  v_assert_packet_cmpstr (packet, "album", ==, "Test Album");
  v_assert_packet_cmpint (packet, "length", ==, 180000);
  json_node_unref (packet);

  /* Request Seek */
  packet = valent_test_fixture_lookup_packet (fixture, "request-seek");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect position of 1s */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpint (packet, "pos", ==, 1000);
  json_node_unref (packet);

  /* Request Stop */
  packet = valent_test_fixture_lookup_packet (fixture, "request-stop");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect quiescent state */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_false (packet, "canPause");
  v_assert_packet_false (packet, "canPlay");
  v_assert_packet_false (packet, "canGoNext");
  v_assert_packet_false (packet, "canGoPrevious");
  v_assert_packet_false (packet, "canSeek");
  v_assert_packet_false (packet, "isPlaying");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "None");
  v_assert_packet_false (packet, "shuffle");

  v_assert_packet_no_field (packet, "artist");
  v_assert_packet_no_field (packet, "title");
  v_assert_packet_no_field (packet, "album");
  v_assert_packet_no_field (packet, "length");
  json_node_unref (packet);

  /* Request repeat change */
  packet = valent_test_fixture_lookup_packet (fixture, "request-repeat");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect repeat change */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "Track");
  json_node_unref (packet);

  /* Request shuffle change */
  packet = valent_test_fixture_lookup_packet (fixture, "request-shuffle");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect shuffle change */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_true (packet, "shuffle");
  json_node_unref (packet);

  /* Request volume change */
  packet = valent_test_fixture_lookup_packet (fixture, "request-volume");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect volume change */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpint (packet, "volume", ==, 50);
  json_node_unref (packet);

  /* Update for album transfer */
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "mpris:artUrl", "s", "file://"TEST_DATA_DIR"/image.png");
  valent_mpris_remote_update_metadata (remote, g_variant_dict_end (&dict));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "albumArtUrl", ==, "file://"TEST_DATA_DIR"/image.png");
  json_node_unref (packet);

  /* Request album art transfer */
  packet = create_albumart_request ("file://"TEST_DATA_DIR"/image.png");
  valent_test_fixture_handle_packet (fixture, packet);
  json_node_unref (packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_fixture_download (fixture, packet, &error);
  g_assert_no_error (error);

  json_node_unref (packet);

  /* ... */
  g_signal_handlers_disconnect_by_data (media, fixture);
}

static void
on_player_changed (ValentMediaPlayer *player,
                   GParamSpec        *pspec,
                   ValentTestFixture *fixture)
{
  valent_test_fixture_quit (fixture);
}

static void
valent_mpris_player_new_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ValentTestFixture *fixture = user_data;
  g_autoptr (GError) error = NULL;

  proxy = valent_mpris_player_new_finish (result, &error);
  g_assert_no_error (error);

  g_signal_connect (proxy,
                    "notify::metadata",
                    G_CALLBACK (on_player_changed),
                    fixture);
}

static void
on_name_owner_changed (GDBusConnection *connection,
                       const char      *sender_name,
                       const char      *object_path,
                       const char      *interface_name,
                       const char      *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  ValentTestFixture *fixture = user_data;
  const char *name, *old_owner, *new_owner;

  g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

  if (strlen (new_owner) > 0)
    valent_mpris_player_new (name, NULL, valent_mpris_player_new_cb, fixture);

  else if (strlen (old_owner) > 0)
    g_clear_object (&proxy);
}

static void
test_mpris_plugin_handle_player (ValentTestFixture *fixture,
                                 gconstpointer      user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GFile) file = NULL;
  unsigned int watch_id;
  JsonNode *packet;
  GError *error = NULL;
  g_autoptr (GVariant) metadata = NULL;
  const char **artist;
  const char *title;
  const char *album;
  gint64 length;

  /* Watch for exported player */
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  watch_id = g_dbus_connection_signal_subscribe (connection,
                                                 "org.freedesktop.DBus",
                                                 "org.freedesktop.DBus",
                                                 "NameOwnerChanged",
                                                 "/org/freedesktop/DBus",
                                                 "org.mpris.MediaPlayer2",
                                                 G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
                                                 on_name_owner_changed,
                                                 fixture, NULL);

  /* Expect connect-time packets */
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_true (packet, "requestPlayerList");
  json_node_unref (packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_field (packet, "playerList");
  json_node_unref (packet);

  /* Send player list */
  packet = valent_test_fixture_lookup_packet (fixture, "player-list");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_true (packet, "requestNowPlaying");
  v_assert_packet_true (packet, "requestVolume");
  json_node_unref (packet);

  /* Wait for player to be exported */
  while (proxy == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* Send quiescent state */
  packet = valent_test_fixture_lookup_packet (fixture, "player-quiescent");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Send playing state */
  packet = valent_test_fixture_lookup_packet (fixture, "player-playing");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Album Art Request */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "albumArtUrl", ==, "/path/to/image.png");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "player-albumart");
  file = g_file_new_for_path (TEST_DATA_DIR"/image.png");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  while (metadata == NULL)
    {
      g_main_context_iteration (NULL, TRUE);
      metadata = valent_media_player_get_metadata (VALENT_MEDIA_PLAYER (proxy));
    }

  g_assert_true (g_variant_lookup (metadata, "xesam:artist", "^a&s", &artist));
  g_assert_true (g_variant_lookup (metadata, "xesam:title", "&s", &title));
  g_assert_true (g_variant_lookup (metadata, "xesam:album", "&s", &album));
  g_assert_true (g_variant_lookup (metadata, "mpris:length", "x", &length));

  g_assert_cmpstr (artist[0], ==, "Test Artist");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (album, ==, "Test Album");
  g_assert_cmpint (length, ==, 180000);
  g_clear_pointer (&artist, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  /* Actions */
  valent_media_player_play (VALENT_MEDIA_PLAYER (proxy));
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Play");
  json_node_unref (packet);

  valent_media_player_pause (VALENT_MEDIA_PLAYER (proxy));
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Pause");
  json_node_unref (packet);

  valent_media_player_stop (VALENT_MEDIA_PLAYER (proxy));
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Stop");
  json_node_unref (packet);

  valent_media_player_next (VALENT_MEDIA_PLAYER (proxy));
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Next");
  json_node_unref (packet);

  valent_media_player_previous (VALENT_MEDIA_PLAYER (proxy));
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Previous");
  json_node_unref (packet);

  /* Seek & SetPosition */
  valent_media_player_seek (VALENT_MEDIA_PLAYER (proxy), 1000);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpint (packet, "Seek", ==, 1);
  json_node_unref (packet);

  valent_media_player_set_position (VALENT_MEDIA_PLAYER (proxy), 1000);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpint (packet, "SetPosition", ==, 1);
  json_node_unref (packet);


  /* Properties */
  valent_media_player_set_repeat (VALENT_MEDIA_PLAYER (proxy), VALENT_MEDIA_REPEAT_ALL);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpstr (packet, "setLoopStatus", ==, "Playlist");
  json_node_unref (packet);

  valent_media_player_set_shuffle (VALENT_MEDIA_PLAYER (proxy), TRUE);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_true (packet, "setShuffle");
  json_node_unref (packet);

  valent_media_player_set_volume (VALENT_MEDIA_PLAYER (proxy), 0.50);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Test Player");
  v_assert_packet_cmpint (packet, "setVolume", ==, 50);
  json_node_unref (packet);

  /* Send empty player list */
  packet = valent_test_fixture_lookup_packet (fixture, "player-list-empty");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Wait for player to be unexported */
  while (proxy != NULL)
    g_main_context_iteration (NULL, FALSE);

  g_dbus_connection_signal_unsubscribe (connection, watch_id);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.mpris.json",
  JSON_SCHEMA_DIR"/kdeconnect.mpris.request.json",
};

static void
test_mpris_plugin_fuzz (ValentTestFixture *fixture,
                        gconstpointer      user_data)

{
  valent_test_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (unsigned int s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-mpris.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mpris/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mpris_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/mpris/handle-player",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mpris_plugin_handle_player,
              valent_test_fixture_clear);

  g_test_add ("/plugins/mpris/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mpris_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
