// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-media.h>
#include <libvalent-test.h>

typedef struct
{
  ValentMixerControl *control;
  ValentMixerStream  *speakers;
  ValentMixerStream  *headphones;
  ValentMixerStream  *microphone;
} MixerInfo;

static void
mixer_info_free (gpointer data)
{
  MixerInfo *info = data;

  g_clear_object (&info->speakers);
  g_clear_object (&info->headphones);
  g_clear_object (&info->microphone);
  g_free (info);
}

static void
telephony_plugin_fixture_set_up (ValentTestPluginFixture *fixture,
                                 gconstpointer            user_data)
{
  MixerInfo *info;
  ValentMixer *mixer;
  ValentMixerControl *control;

  valent_test_plugin_fixture_init (fixture, user_data);

  mixer = valent_mixer_get_default ();
  g_assert_true (VALENT_IS_MIXER (mixer));

  while ((control = valent_mock_mixer_control_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  info = g_new0 (MixerInfo, 1);
  info->control = control;
  info->speakers = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                 "name",        "mock-speakers",
                                 "description", "Mock Speakers",
                                 "flags",       (VALENT_MIXER_STREAM_LOCAL |
                                                 VALENT_MIXER_STREAM_SINK),
                                 "level",       100,
                                 NULL);
  info->headphones = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                   "name",        "mock-headphones",
                                   "description", "Mock Headphones",
                                   "flags",       (VALENT_MIXER_STREAM_LOCAL |
                                                  VALENT_MIXER_STREAM_SINK),
                                   "level",       100,
                                   NULL);
  info->microphone = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                   "name",        "mock-microphone",
                                   "description", "Mock Microphone",
                                   "flags",       (VALENT_MIXER_STREAM_LOCAL |
                                                   VALENT_MIXER_STREAM_SOURCE),
                                   "level",       100,
                                   NULL);
  valent_test_plugin_fixture_set_data (fixture, info, mixer_info_free);

  valent_mixer_control_emit_stream_added (info->control, info->speakers);
  valent_mixer_control_emit_stream_added (info->control, info->microphone);
}

static void
test_telephony_plugin_basic (ValentTestPluginFixture *fixture,
                             gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  g_assert_true (g_action_group_has_action (actions, "mute-call"));
}

static void
test_telephony_plugin_handle_event (ValentTestPluginFixture *fixture,
                                    gconstpointer            user_data)
{
  MixerInfo *info = fixture->data;
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Receive an unanswered call event-chain */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing-cancel");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);

  /* Receive an answered call event-chain */

  /* Receive an answered call event-chain. What we expect is:
   *
   * 1. Phone rings; speaker volume is lowered to %15
   * 3. Phone is answered; speakers are muted,
   *                       microphone is muted
   * 4. Phone is hung-up; speakers are raised to 100% and unmuted,
   *                      microphone is unmuted
   */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "talking");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, TRUE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, TRUE);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "talking-cancel");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);

  /* Receive an answered call event-chain. In this case, emulate inserting
   * headphones after the phone started ringing. Thus what we expect is:
   *
   * 1. Phone rings; speaker volume is lowered to %15
   * 2. Headphones are plugged in
   * 3. Phone is answered; speakers & headphones remain in their current state,
   *                       microphone is muted
   * 4. Phone is hung-up; speakers & headphones remain in their current state,
   *                      microphone is unmuted
   */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  valent_mixer_control_emit_stream_added (info->control, info->headphones);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "talking");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, TRUE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);

  packet = valent_test_plugin_fixture_lookup_packet (fixture, "talking-cancel");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  g_assert_cmpuint (valent_mixer_stream_get_level (info->speakers), ==, 15);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->speakers), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->microphone), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->microphone), ==, FALSE);
  g_assert_cmpuint (valent_mixer_stream_get_level (info->headphones), ==, 100);
  g_assert_cmpuint (valent_mixer_stream_get_muted (info->headphones), ==, FALSE);
}

static void
test_telephony_plugin_mute_call (ValentTestPluginFixture *fixture,
                                 gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;
  JsonNode *packet;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Receive a ringing event */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Mute the call */
  g_action_group_activate_action (actions, "mute-call", NULL);
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.telephony.request_mute");
  json_node_unref (packet);

  /* Cancel ringing */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "ringing-cancel");
  valent_test_plugin_fixture_handle_packet (fixture, packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.telephony.json",
  JSON_SCHEMA_DIR"/kdeconnect.telephony.request_mute.json",
};

static void
test_telephony_plugin_fuzz (ValentTestPluginFixture *fixture,
                            gconstpointer            user_data)

{
  valent_test_plugin_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (unsigned int s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_plugin_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-telephony.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/telephony/basic",
              ValentTestPluginFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/telephony/handle-event",
              ValentTestPluginFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_handle_event,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/telephony/mute-call",
              ValentTestPluginFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_mute_call,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/telephony/fuzz",
              ValentTestPluginFixture, path,
              telephony_plugin_fixture_set_up,
              test_telephony_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
