// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


typedef struct
{
  ValentMixerControl *control;
  ValentMixerStream  *sink1;
  ValentMixerStream  *sink2;
} MixerInfo;

static void
mixer_info_free (gpointer data)
{
  MixerInfo *info = data;

  g_clear_object (&info->sink1);
  g_clear_object (&info->sink2);
  g_free (info);
}

static void
systemvolume_plugin_fixture_set_up (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  MixerInfo *info;
  ValentMixer *mixer;
  ValentMixerControl *control;

  valent_test_fixture_init (fixture, user_data);

  mixer = valent_mixer_get_default ();
  g_assert_true (VALENT_IS_MIXER (mixer));

  while ((control = valent_mock_mixer_control_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  info = g_new0 (MixerInfo, 1);
  info->control = control;
  info->sink1 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                              "name",        "test_sink1",
                              "description", "Test Speakers",
                              "flags",       (VALENT_MIXER_STREAM_LOCAL |
                                              VALENT_MIXER_STREAM_SINK),
                              NULL);
  info->sink2 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                              "name",        "test_sink2",
                              "description", "Test Headphones",
                              "flags",       (VALENT_MIXER_STREAM_LOCAL |
                                              VALENT_MIXER_STREAM_SINK),
                              NULL);
  valent_test_fixture_set_data (fixture, info, mixer_info_free);
}

static void
test_systemvolume_plugin_handle_request (ValentTestFixture *fixture,
                                         gconstpointer      user_data)
{
  MixerInfo *info = fixture->data;
  JsonNode *packet;

  valent_mixer_control_emit_stream_added (info->control, info->sink1);

  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  json_node_unref (packet);

  /* Request the sink list */
  packet = valent_test_fixture_lookup_packet (fixture, "request-sinks");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  json_node_unref (packet);

  /* Receive a systemvolume */
  packet = valent_test_fixture_lookup_packet (fixture, "request-mute");
  valent_test_fixture_handle_packet (fixture, packet);
  g_assert_true (valent_mixer_stream_get_muted (info->sink1));

  /* Receive a systemvolume (message) */
  packet = valent_test_fixture_lookup_packet (fixture, "request-volume");
  valent_test_fixture_handle_packet (fixture, packet);
  g_assert_cmpint (valent_mixer_stream_get_level (info->sink1), ==, 50);

  /* Expect a stream updates */
  valent_mixer_stream_set_level (info->sink1, 100);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_true (packet, "muted");
  v_assert_packet_cmpint (packet, "volume", ==, 100);
  json_node_unref (packet);

  valent_mixer_stream_set_muted (info->sink1, FALSE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_false (packet, "muted");
  v_assert_packet_cmpint (packet, "volume", ==, 100);
  json_node_unref (packet);

  /* Expect ... */
  valent_mixer_control_emit_stream_added (info->control, info->sink2);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  json_node_unref (packet);

  valent_mixer_control_emit_stream_removed (info->control, info->sink2);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  json_node_unref (packet);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-systemvolume.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/systemvolume/handle-request",
              ValentTestFixture, path,
              systemvolume_plugin_fixture_set_up,
              test_systemvolume_plugin_handle_request,
              valent_test_fixture_clear);

  return g_test_run ();
}
