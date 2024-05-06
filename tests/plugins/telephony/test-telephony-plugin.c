// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"

typedef struct
{
  ValentMixerAdapter *adapter;
  ValentMixerStream  *speakers;
  ValentMixerStream  *headphones;
  ValentMixerStream  *microphone;
  ValentMedia        *media;
  ValentMediaPlayer  *player1;
  ValentMediaPlayer  *player2;
} MixerInfo;

static void
mixer_info_free (gpointer data)
{
  MixerInfo *info = data;

  /* NOTE: we need to finalize the singletons between tests */
  v_assert_finalize_object (valent_mixer_get_default ());
  v_assert_finalize_object (valent_media_get_default ());

  g_clear_object (&info->speakers);
  g_clear_object (&info->headphones);
  g_clear_object (&info->microphone);
  g_clear_object (&info->player1);
  g_clear_object (&info->player2);
  g_free (info);
}

static void
telephony_plugin_fixture_set_up (ValentTestFixture *fixture,
                                 gconstpointer      user_data)
{
  MixerInfo *info;

  valent_test_fixture_init (fixture, user_data);

  info = g_new0 (MixerInfo, 1);
  info->adapter = valent_test_await_adapter (valent_mixer_get_default ());
  info->speakers = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                 "name",        "mock-speakers",
                                 "description", "Mock Speakers",
                                 "direction",   VALENT_MIXER_OUTPUT,
                                 "level",       100,
                                 NULL);
  info->headphones = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                   "name",        "mock-headphones",
                                   "description", "Mock Headphones",
                                   "direction",   VALENT_MIXER_OUTPUT,
                                   "level",       100,
                                   NULL);
  info->microphone = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                   "name",        "mock-microphone",
                                   "description", "Mock Microphone",
                                   "direction",   VALENT_MIXER_INPUT,
                                   "level",       100,
                                   NULL);
  info->player1 = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  info->player2 = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  valent_test_fixture_set_data (fixture, info, mixer_info_free);

  valent_mixer_adapter_stream_added (info->adapter, info->speakers);
  valent_mixer_adapter_stream_added (info->adapter, info->microphone);
  valent_mixer_adapter_stream_added (info->adapter, info->headphones);
  valent_media_export_player (valent_media_get_default (), info->player1);
  valent_media_export_player (valent_media_get_default (), info->player2);
}

static void
test_telephony_plugin_basic (ValentTestFixture *fixture,
                             gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "telephony.mute-call"));

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin action `telephony.mute-call` is enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "telephony.mute-call"));
}

static void
test_telephony_plugin_handle_event (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  MixerInfo *info = fixture->data;
  JsonNode *packet;
  gboolean watch = FALSE;

  valent_media_player_play (info->player1);
  valent_test_watch_signal (info->speakers, "notify", &watch);
  valent_test_watch_signal (info->microphone, "notify", &watch);
  valent_test_fixture_connect (fixture, TRUE);

  /* Receive an unanswered call event-chain. What we expect is:
   *
   * 1. Phone rings
   *    i. speaker volume is lowered to %15
   * 2. Phone is unanswered
   *    i. speakers are raised to 100%
   */
  VALENT_TEST_CHECK ("Plugin handles a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "ringing");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PLAYING);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  VALENT_TEST_CHECK ("Plugin handles a `isCancel` event, following a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "ringing-cancel");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PLAYING);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  /* Receive an answered call event-chain. What we expect is:
   *
   * 1. Phone rings
   *    i. speaker volume is lowered to %15
   * 2. Phone is answered
   *    i. speakers are muted
   *    ii. microphone is muted
   *    iii. media is paused
   * 3. Phone is hung-up
   *    i. speakers are raised to 100% and unmuted
   *    ii. microphone is unmuted
   *    iii. media is unpaused
   */
  VALENT_TEST_CHECK ("Plugin handles a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "ringing");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PLAYING);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  VALENT_TEST_CHECK ("Plugin handles a `talking` event, following a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "talking");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, TRUE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, TRUE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PAUSED);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  VALENT_TEST_CHECK ("Plugin handles a `isCancel` event, following a `talking` event");
  packet = valent_test_fixture_lookup_packet (fixture, "talking-cancel");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PLAYING);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  valent_test_watch_clear (info->speakers, &watch);
  valent_test_watch_clear (info->microphone, &watch);
}

static void
test_telephony_plugin_handle_mixer (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  MixerInfo *info = fixture->data;
  JsonNode *packet;
  gboolean watch = FALSE;

  valent_media_player_play (info->player1);
  valent_test_watch_signal (info->speakers, "notify", &watch);
  valent_test_watch_signal (info->microphone, "notify", &watch);
  valent_test_fixture_connect (fixture, TRUE);

  /* Receive an answered call event-chain. In this case, emulate inserting
   * headphones after the phone started ringing. Thus what we expect is:
   *
   * 1. Phone rings
   *    i. speaker volume is lowered to %15
   * 2. Phone is answered
   *    i. speakers remain unchanged
   *    ii. microphone is muted
   *    iii. media is paused
   * 3. Phone is hung-up
   *    i. speakers remain unchanged
   *    ii. microphone is unmuted
   *    iii. media is unpaused
   */
  VALENT_TEST_CHECK ("Plugin handles a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "ringing");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PLAYING);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  /* User inserts headphones */
  valent_mixer_adapter_set_default_output (info->adapter, info->headphones);

  VALENT_TEST_CHECK ("Plugin handles an audio change between a `ringing` and `talking` event");
  packet = valent_test_fixture_lookup_packet (fixture, "talking");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, TRUE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PAUSED);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  VALENT_TEST_CHECK ("Plugin handles a `isCancel` event, following a `talking` event");
  packet = valent_test_fixture_lookup_packet (fixture, "talking-cancel");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PLAYING);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  valent_test_watch_clear (info->speakers, &watch);
  valent_test_watch_clear (info->microphone, &watch);
}

static void
test_telephony_plugin_handle_media (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  MixerInfo *info = fixture->data;
  JsonNode *packet;
  gboolean watch = FALSE;

  valent_media_player_play (info->player1);
  valent_test_watch_signal (info->speakers, "notify", &watch);
  valent_test_watch_signal (info->microphone, "notify", &watch);
  valent_test_fixture_connect (fixture, TRUE);

  /* Receive an answered call event-chain. In this case, emulate stopping a
   * paused player after the phone is answered. Thus what we expect is:
   *
   * 1. Phone rings
   *    i. speaker volume is lowered to %15
   * 2. Phone is answered
   *    i. speakers are muted
   *    ii. microphone is muted
   *    iii. media is paused
   * 3. Phone is hung-up
   *    i. speakers are raised to 100% and unmuted
   *    ii. microphone is unmuted
   *    iii. media is stopped
   */
  VALENT_TEST_CHECK ("Plugin handles a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "ringing");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PLAYING);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  VALENT_TEST_CHECK ("Plugin handles a `talking` event, following a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "talking");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, TRUE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, TRUE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_PAUSED);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  /* User stops player */
  valent_media_player_stop (info->player1);

  VALENT_TEST_CHECK ("Plugin handles a `isCancel` event, following a `talking` event");
  packet = valent_test_fixture_lookup_packet (fixture, "talking-cancel");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
  g_assert_cmpuint (valent_media_player_get_state (info->player1), ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpuint (valent_media_player_get_state (info->player2), ==, VALENT_MEDIA_STATE_STOPPED);

  valent_test_watch_clear (info->speakers, &watch);
  valent_test_watch_clear (info->microphone, &watch);
}

static void
test_telephony_plugin_mute_call (ValentTestFixture *fixture,
                                 gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin handles a `ringing` event");
  packet = valent_test_fixture_lookup_packet (fixture, "ringing");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin action `telephony.mute-call` sends a request to stop ringing");
  g_action_group_activate_action (actions, "telephony.mute-call", NULL);
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.telephony.request_mute");
  json_node_unref (packet);

  /* Cancel ringing */
  packet = valent_test_fixture_lookup_packet (fixture, "ringing-cancel");
  valent_test_fixture_handle_packet (fixture, packet);
}

static const char *schemas[] = {
  "/tests/kdeconnect.telephony.json",
  "/tests/kdeconnect.telephony.request_mute.json",
};

static void
test_telephony_plugin_fuzz (ValentTestFixture *fixture,
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
  const char *path = "plugin-telephony.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/telephony/basic",
              ValentTestFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/telephony/handle-event",
              ValentTestFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_handle_event,
              valent_test_fixture_clear);

  g_test_add ("/plugins/telephony/handle-mixer",
              ValentTestFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_handle_mixer,
              valent_test_fixture_clear);

  g_test_add ("/plugins/telephony/handle-media",
              ValentTestFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_handle_media,
              valent_test_fixture_clear);

  g_test_add ("/plugins/telephony/mute-call",
              ValentTestFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_mute_call,
              valent_test_fixture_clear);

  g_test_add ("/plugins/telephony/fuzz",
              ValentTestFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
