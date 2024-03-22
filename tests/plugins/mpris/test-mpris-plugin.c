// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"
#include "valent-mpris-impl.h"
#include "valent-mpris-player.h"


static void
export_cb (ValentMPRISImpl   *impl,
           GAsyncResult      *result,
           ValentTestFixture *fixture)
{
  g_autoptr (GError) error = NULL;

  valent_mpris_impl_export_finish (impl, result, &error);
  g_assert_no_error (error);
}

static JsonNode *
create_albumart_request (const char *art_url)
{
  g_autoptr (JsonBuilder) builder = NULL;

  valent_packet_init (&builder, "kdeconnect.mpris.request");
  json_builder_set_member_name (builder, "player");
  json_builder_add_string_value (builder, "Mock Player");
  json_builder_set_member_name (builder, "albumArtUrl");
  json_builder_add_string_value (builder, art_url);

  return valent_packet_end (&builder);
}

static void
text_mpris_plugin_fixture_clear (ValentTestFixture *fixture,
                                 gconstpointer      user_data)
{
  ValentMedia *media = valent_media_get_default ();

  valent_test_fixture_clear (fixture, user_data);
  v_await_finalize_object (media);
}

static void
test_mpris_plugin_handle_request (ValentTestFixture *fixture,
                                  gconstpointer      user_data)
{
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;
  g_autoptr (GError) error = NULL;
  JsonNode *packet;
  JsonArray *player_list;
  const char *player_name;

  /* Export a mock player that we can use to poke the plugin during testing */
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  impl = valent_mpris_impl_new (player);
  valent_mpris_impl_export_full (impl,
                                 "org.mpris.MediaPlayer2.Test",
                                 NULL,
                                 (GAsyncReadyCallback)export_cb,
                                 fixture);
  valent_test_await_signal (valent_media_get_default (), "items-changed");

  VALENT_TEST_CHECK ("Plugin requests the list of players on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_true (packet, "requestPlayerList");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the list of players on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  player_list = json_object_get_array_member (valent_packet_get_body (packet),
                                              "playerList");
  player_name = json_array_get_string_element (player_list, 0);
  g_assert_cmpstr (player_name, ==, "Mock Player");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the list of players when requested");
  packet = valent_test_fixture_lookup_packet (fixture, "request-now-playing");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin sends players with the expected properties");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
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
  VALENT_TEST_CHECK ("Plugin responds to a request to Play");
  packet = valent_test_fixture_lookup_packet (fixture, "request-play");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
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

  VALENT_TEST_CHECK ("Plugin responds to a request to Go Next");
  packet = valent_test_fixture_lookup_packet (fixture, "request-next");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect Track 2 */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
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

  VALENT_TEST_CHECK ("Plugin responds to a request to Go Previous");
  packet = valent_test_fixture_lookup_packet (fixture, "request-previous");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect Track 1 */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
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

  VALENT_TEST_CHECK ("Plugin responds to a request to Pause");
  packet = valent_test_fixture_lookup_packet (fixture, "request-pause");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect paused state */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
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

  VALENT_TEST_CHECK ("Plugin responds to a request to Seek");
  packet = valent_test_fixture_lookup_packet (fixture, "request-seek");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect position of 1s */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpint (packet, "pos", ==, 1000);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin responds to a request to Stop");
  packet = valent_test_fixture_lookup_packet (fixture, "request-stop");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin responds with an update that the position is reset");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpint (packet, "pos", ==, 0);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin responds with an update that the player is quiescent");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");

  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_false (packet, "canPause");
  v_assert_packet_true (packet, "canPlay");
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

  VALENT_TEST_CHECK ("Plugin responds to a request to change Loop Status");
  packet = valent_test_fixture_lookup_packet (fixture, "request-repeat");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect repeat change */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "loopStatus", ==, "Track");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin responds to a request to change Shuffle");
  packet = valent_test_fixture_lookup_packet (fixture, "request-shuffle");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect shuffle change */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_true (packet, "shuffle");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin responds to a request to change Volume");
  packet = valent_test_fixture_lookup_packet (fixture, "request-volume");
  valent_test_fixture_handle_packet (fixture, packet);

  /* Expect volume change */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpint (packet, "volume", ==, 50);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin send updates for Album Art");
  valent_mock_media_player_update_art (VALENT_MOCK_MEDIA_PLAYER (player),
                                       "resource:///tests/image.png");

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "albumArtUrl", ==, "resource:///tests/image.png");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin responds to a request to transfer Album Art");
  packet = create_albumart_request ("resource:///tests/image.png");
  valent_test_fixture_handle_packet (fixture, packet);
  json_node_unref (packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_fixture_download (fixture, packet, &error);
  g_assert_no_error (error);

  json_node_unref (packet);
}

static void
g_async_initable_new_async_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  ValentMediaPlayer *player = VALENT_MEDIA_PLAYER (object);
  ValentTestFixture *fixture = user_data;
  g_autoptr (GObject) ret = NULL;
  g_autoptr (GError) error = NULL;

  ret = g_async_initable_new_finish (G_ASYNC_INITABLE (object), result, &error);
  g_assert_no_error (error);

  valent_test_fixture_set_data (fixture, g_object_ref (player), g_object_unref);
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
    g_async_initable_new_async (VALENT_TYPE_MPRIS_PLAYER,
                                G_PRIORITY_DEFAULT,
                                NULL,
                                g_async_initable_new_async_cb,
                                fixture,
                                "bus-name", name,
                                NULL);

  else if (strlen (old_owner) > 0)
    g_clear_object (&fixture->data);
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
  int64_t length;

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

  VALENT_TEST_CHECK ("Plugin requests the list of players on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_true (packet, "requestPlayerList");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the list of players on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris");
  v_assert_packet_field (packet, "playerList");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles a list of players");
  packet = valent_test_fixture_lookup_packet (fixture, "player-list");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_true (packet, "requestNowPlaying");
  v_assert_packet_true (packet, "requestVolume");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin exports listed players");
  valent_test_await_pointer (&fixture->data);

  VALENT_TEST_CHECK ("Plugin handles Stopped state");
  packet = valent_test_fixture_lookup_packet (fixture, "player-quiescent");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles Playing state");
  packet = valent_test_fixture_lookup_packet (fixture, "player-playing");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin request Album Art when offered");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "albumArtUrl", ==, "resource:///tests/image.png");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "player-albumart");
  file = g_file_new_for_uri ("resource:///tests/image.png");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  VALENT_TEST_CHECK ("Plugin updates exported player with Album Art");
  valent_test_await_signal (fixture->data, "notify::metadata");

  metadata = valent_media_player_get_metadata (fixture->data);
  g_assert_true (g_variant_lookup (metadata, "xesam:artist", "^a&s", &artist));
  g_assert_true (g_variant_lookup (metadata, "xesam:title", "&s", &title));
  g_assert_true (g_variant_lookup (metadata, "xesam:album", "&s", &album));
  g_assert_true (g_variant_lookup (metadata, "mpris:length", "x", &length));

  g_assert_cmpstr (artist[0], ==, "Test Artist");
  g_assert_cmpstr (title, ==, "Test Title");
  g_assert_cmpstr (album, ==, "Test Album");
  g_assert_cmpint (length, ==, 180000000);
  g_clear_pointer (&artist, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  VALENT_TEST_CHECK ("Plugin forwards request to Play");
  valent_media_player_play (fixture->data);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Play");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards request to Pause");
  valent_media_player_pause (fixture->data);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Pause");
  json_node_unref (packet);

#if 0
  VALENT_TEST_CHECK ("Plugin forwards request to PlayPause");
  valent_media_player_play_pause (fixture->data);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "action", ==, "PlayPause");
  json_node_unref (packet);
#endif

  VALENT_TEST_CHECK ("Plugin forwards request to Stop");
  valent_media_player_stop (fixture->data);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Stop");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards request to Go Next");
  valent_media_player_next (fixture->data);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Next");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards request to Go Previous");
  valent_media_player_previous (fixture->data);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "action", ==, "Previous");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards request to Seek");
  valent_media_player_seek (fixture->data, 1.0);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpint (packet, "Seek", ==, G_TIME_SPAN_SECOND);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards request to SetPosition");
  valent_media_player_set_position (fixture->data, 1.0);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpint (packet, "SetPosition", ==, 1000);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  valent_media_player_set_repeat (fixture->data, VALENT_MEDIA_REPEAT_ALL);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpstr (packet, "setLoopStatus", ==, "Playlist");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards request to change Shuffle");
  valent_media_player_set_shuffle (fixture->data, TRUE);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_true (packet, "setShuffle");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards request to change Volume");
  valent_media_player_set_volume (fixture->data, 0.50);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mpris.request");
  v_assert_packet_cmpstr (packet, "player", ==, "Mock Player");
  v_assert_packet_cmpint (packet, "setVolume", ==, 50);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles empty player list");
  packet = valent_test_fixture_lookup_packet (fixture, "player-list-empty");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin unexports unlisted players");
  valent_test_await_nullptr (&fixture->data);

  g_dbus_connection_signal_unsubscribe (connection, watch_id);
}

static const char *schemas[] = {
  "/tests/kdeconnect.mpris.json",
  "/tests/kdeconnect.mpris.request.json",
};

static void
test_mpris_plugin_fuzz (ValentTestFixture *fixture,
                        gconstpointer      user_data)

{
  valent_test_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (size_t s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mpris.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mpris/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mpris_plugin_handle_request,
              text_mpris_plugin_fixture_clear);

  g_test_add ("/plugins/mpris/handle-player",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mpris_plugin_handle_player,
              text_mpris_plugin_fixture_clear);

  g_test_add ("/plugins/mpris/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mpris_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
