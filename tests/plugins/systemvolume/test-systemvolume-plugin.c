// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
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

  g_clear_object (&info->sink1);
  g_clear_object (&info->sink2);
  g_free (info);
}

static void
systemvolume_plugin_fixture_set_up (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  MixerInfo *info;

  valent_test_fixture_init (fixture, user_data);

  info = g_new0 (MixerInfo, 1);
  info->adapter = valent_test_await_adapter (valent_mixer_get_default ());
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
  gboolean watch = FALSE;
  gboolean watch_level = FALSE;
  gboolean watch_muted = FALSE;

  valent_mixer_adapter_stream_added (info->adapter, info->sink1);
  valent_test_watch_signal (info->adapter, "notify::default-output", &watch);
  valent_test_watch_signal (info->sink1, "notify::level", &watch_level);
  valent_test_watch_signal (info->sink1, "notify::muted", &watch_muted);

  VALENT_TEST_CHECK ("Plugin sends the sink list on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 1);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the sink list when requested");
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

  VALENT_TEST_CHECK ("Plugin responds to a request to mute a stream");
  packet = valent_test_fixture_lookup_packet (fixture, "request-mute");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_boolean (&watch_muted);
  g_assert_true (valent_mixer_stream_get_muted (info->sink1));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_true (packet, "muted");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends an update when a stream is muted or unmuted");
  valent_mixer_stream_set_muted (info->sink1, FALSE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_false (packet, "muted");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin responds to a request to change the volume of a stream");
  packet = valent_test_fixture_lookup_packet (fixture, "request-volume");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_boolean (&watch_level);
  g_assert_cmpint (valent_mixer_stream_get_level (info->sink1), ==, 50);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_cmpint (packet, "volume", ==, 50);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends an update when a stream's volume is changed");
  valent_mixer_stream_set_level (info->sink1, 100);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_cmpstr (packet, "name", ==, "test_sink1");
  v_assert_packet_cmpint (packet, "volume", ==, 100);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the sink list when a stream is added");
  valent_mixer_adapter_stream_added (info->adapter, info->sink2);

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

  VALENT_TEST_CHECK ("Plugin handles a request to change the default output");
  packet = valent_test_fixture_lookup_packet (fixture, "request-enabled2");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_boolean (&watch);
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

  VALENT_TEST_CHECK ("Plugin handles a request to change the default output");
  packet = valent_test_fixture_lookup_packet (fixture, "request-enabled1");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_boolean (&watch);
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

  VALENT_TEST_CHECK ("Plugin sends the sink list when a stream is removed");
  valent_mixer_adapter_stream_removed (info->adapter, info->sink2);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 1);
  sink_info = json_array_get_object_element (sink_list, 0);
  g_assert_cmpstr (json_object_get_string_member (sink_info, "name"), ==, "test_sink1");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the sink list when a stream is missing");
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

  valent_test_watch_clear (info->adapter, &watch);
  valent_test_watch_clear (info->sink1, &watch_level);
  valent_test_watch_clear (info->sink1, &watch_muted);
}

static void
test_systemvolume_plugin_handle_sinks (ValentTestFixture *fixture,
                                       gconstpointer      user_data)
{
  g_autoptr (ValentMixerAdapter) adapter = NULL;
  g_autoptr (ValentMixerStream) stream = NULL;
  ValentMixerStream *default_output = NULL;
  JsonNode *packet;
  JsonArray *sink_list;
  gboolean watch = FALSE;
  gboolean stream_watch = FALSE;

  adapter = g_list_model_get_item (G_LIST_MODEL (valent_mixer_get_default ()), 1);
  valent_test_watch_signal (adapter, "notify::default-output", &watch);

  VALENT_TEST_CHECK ("Plugin sends the sink list on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume");
  v_assert_packet_field (packet, "sinkList");
  g_assert_true (valent_packet_get_array (packet, "sinkList", &sink_list));
  g_assert_cmpuint (json_array_get_length (sink_list), ==, 0);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the sink list");
  packet = valent_test_fixture_lookup_packet (fixture, "sinklist-1");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_boolean (&watch);
  g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), ==, 1);

  VALENT_TEST_CHECK ("Plugin exports the sinks");
  default_output = valent_mixer_adapter_get_default_output (adapter);
  g_assert_true (VALENT_IS_MIXER_STREAM (default_output));
  g_assert_cmpuint (valent_mixer_stream_get_level (default_output), ==, 50);
  g_assert_false (valent_mixer_stream_get_muted (default_output));

  VALENT_TEST_CHECK ("Plugin forwards volume change requests");
  valent_mixer_stream_set_level (default_output, 100);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume.request");
  v_assert_packet_cmpstr (packet, "name", ==, "mock.speakers.analog-stereo");
  v_assert_packet_cmpint (packet, "volume", ==, 65536);
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "sinklist-1-volume");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_watch_signal (default_output, "notify::level", &stream_watch);
  valent_test_await_boolean (&stream_watch);
  valent_test_watch_clear (default_output, &stream_watch);

  VALENT_TEST_CHECK ("Plugin forwards muted change requests");
  valent_mixer_stream_set_muted (default_output, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume.request");
  v_assert_packet_cmpstr (packet, "name", ==, "mock.speakers.analog-stereo");
  v_assert_packet_true (packet, "muted");
  json_node_unref (packet);

  packet = valent_test_fixture_lookup_packet (fixture, "sinklist-1-muted");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_watch_signal (default_output, "notify::muted", &stream_watch);
  valent_test_await_boolean (&stream_watch);
  valent_test_watch_clear (default_output, &stream_watch);

  VALENT_TEST_CHECK ("Plugin handles adding streams");
  packet = valent_test_fixture_lookup_packet (fixture, "sinklist-2");
  valent_test_fixture_handle_packet (fixture, packet);

  while (g_list_model_get_n_items (G_LIST_MODEL (adapter)) != 2)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Plugin forwards default output change requests");
  stream = g_list_model_get_item (G_LIST_MODEL (adapter), 1);
  valent_mixer_adapter_set_default_output (adapter, stream);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.systemvolume.request");
  v_assert_packet_cmpstr (packet, "name", ==, "mock.headphones.analog-stereo");
  v_assert_packet_true (packet, "enabled");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the default output update");
  packet = valent_test_fixture_lookup_packet (fixture, "sinklist-2-default");
  valent_test_fixture_handle_packet (fixture, packet);

  valent_test_await_boolean (&watch);
  g_assert_true (stream == valent_mixer_adapter_get_default_output (adapter));

  VALENT_TEST_CHECK ("Plugin handles removing sinks");
  packet = valent_test_fixture_lookup_packet (fixture, "sinklist-3");
  valent_test_fixture_handle_packet (fixture, packet);

  while (g_list_model_get_n_items (G_LIST_MODEL (adapter)) != 1)
    g_main_context_iteration (NULL, FALSE);

  valent_test_watch_clear (default_output, &watch);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-systemvolume.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/systemvolume/handle-request",
              ValentTestFixture, path,
              systemvolume_plugin_fixture_set_up,
              test_systemvolume_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/systemvolume/handle-sinks",
              ValentTestFixture, path,
              systemvolume_plugin_fixture_set_up,
              test_systemvolume_plugin_handle_sinks,
              valent_test_fixture_clear);

  return g_test_run ();
}
