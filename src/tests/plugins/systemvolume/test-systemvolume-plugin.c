// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


typedef struct
{
  ValentMixerAdapter *adapter;
  ValentMixerStream  *sink1;
  ValentMixerStream  *sink2;
} MixerInfo;

static void
mixer_info_free (gpointer data)
{
  MixerInfo *info = data;

  /* NOTE: we need to finalize the mixer singleton between tests */
  v_assert_finalize_object (valent_mixer_get_default ());
  v_await_finalize_object (info->adapter);

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
  ValentMixerAdapter *adapter;

  valent_test_fixture_init (fixture, user_data);

  mixer = valent_mixer_get_default ();
  g_assert_true (VALENT_IS_MIXER (mixer));

  while ((adapter = valent_mock_mixer_adapter_get_instance ()) == NULL)
    g_main_context_iteration (NULL, FALSE);

  info = g_new0 (MixerInfo, 1);
  info->adapter = g_object_ref (adapter);
  info->sink1 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                              "name",        "test_sink1",
                              "description", "Test Speakers",
                              "direction",   VALENT_MIXER_OUTPUT,
                              "level",       100,
                              "muted",       FALSE,
                              NULL);
  info->sink2 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                              "name",        "test_sink2",
                              "description", "Test Headphones",
                              "direction",   VALENT_MIXER_OUTPUT,
                              "level",       100,
                              "muted",       FALSE,
                              NULL);
  valent_test_fixture_set_data (fixture, info, mixer_info_free);
}

static void
test_systemvolume_plugin_handle_request (ValentTestFixture *fixture,
                                         gconstpointer      user_data)
{
  MixerInfo *info = fixture->data;
  JsonNode *packet;
  JsonArray *sink_list;
  JsonObject *sink_info;

  valent_mixer_adapter_emit_stream_added (info->adapter, info->sink1);

  valent_test_fixture_connect (fixture, TRUE);

  /* Expect list of sinks upon connection */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 1);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  json_node_unref (packet);

  /* Request the sink list */
  packet = valent_test_fixture_lookup_packet (fixture, "request-sinks");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 1);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  json_node_unref (packet);

  /* Expect confirmation of a request to change the mute state */
  packet = valent_test_fixture_lookup_packet (fixture, "request-mute");
  valent_test_fixture_handle_packet (fixture, packet);
  g_assert_true (valent_mixer_stream_get_muted (info->sink1));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_true (packet, "muted");
  json_node_unref (packet);

  /* Expect notification of changes to the muted state */
  valent_mixer_stream_set_muted (info->sink1, FALSE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_false (packet, "muted");
  json_node_unref (packet);

  /* Expect confirmation of a request to change the volume level */
  packet = valent_test_fixture_lookup_packet (fixture, "request-volume");
  valent_test_fixture_handle_packet (fixture, packet);
  g_assert_cmpint (valent_mixer_stream_get_level (info->sink1), ==, 50);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_cmpint (packet, "volume", ==, 50);
  json_node_unref (packet);

  /* Expect notification of changes to the volume level */
  valent_mixer_stream_set_level (info->sink1, 100);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_cmpint (packet, "volume", ==, 100);
  json_node_unref (packet);

  /* Expect notification of added streams */
  valent_mixer_adapter_emit_stream_added (info->adapter, info->sink2);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 2);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  sink_info = json_array_get_object_element (sink_list, 1);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink2");
  json_node_unref (packet);

  /* Expect confirmation of a request to change the default stream */
  packet = valent_test_fixture_lookup_packet (fixture, "request-enabled2");
  valent_test_fixture_handle_packet (fixture, packet);
  g_assert_true (valent_mixer_adapter_get_default_output (info->adapter) == info->sink2);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 2);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  sink_info = json_array_get_object_element (sink_list, 1);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink2");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "request-enabled1");
  valent_test_fixture_handle_packet (fixture, packet);
  g_assert_true (valent_mixer_adapter_get_default_output (info->adapter) == info->sink1);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 2);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  sink_info = json_array_get_object_element (sink_list, 1);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink2");
  json_node_unref (packet);

  /* Expect notification of removed streams */
  valent_mixer_adapter_emit_stream_removed (info->adapter, info->sink2);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 1);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  json_node_unref (packet);

  /* Expect to be corrected for an invalid stream request */
  packet = valent_test_fixture_lookup_packet (fixture, "request-enabled2");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 1);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  json_node_unref (packet);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-systemvolume.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/systemvolume/handle-request",
              ValentTestFixture, path,
              systemvolume_plugin_fixture_set_up,
              test_systemvolume_plugin_handle_request,
              valent_test_fixture_clear);

  return g_test_run ();
}
