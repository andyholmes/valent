// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-mixer.h>
#include <libvalent-test.h>


typedef struct
{
  ValentMixer       *mixer;
  ValentMixerStream *input;
  ValentMixerStream *output;
  gpointer           data;
} MixerComponentFixture;

static void
on_stream_added (GObject               *object,
                 ValentMixerStream     *stream,
                 MixerComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_stream_changed (GObject               *object,
                   ValentMixerStream     *stream,
                   MixerComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_stream_removed (GObject               *object,
                   ValentMixerStream     *stream,
                   MixerComponentFixture *fixture)
{
  fixture->data = object;
}

static void
mixer_component_fixture_set_up (MixerComponentFixture *fixture,
                                gconstpointer          user_data)
{
  fixture->mixer = valent_mixer_get_default ();

  fixture->input = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                 "name",        "test_source",
                                 "description", "Test Microphone",
                                 "flags",       VALENT_MIXER_STREAM_SOURCE,
                                 "level",       50,
                                 "muted",       TRUE,
                                 NULL);
  fixture->output = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                  "name",        "test_sink",
                                  "description", "Test Speakers",
                                  "flags", VALENT_MIXER_STREAM_SINK,
                                  NULL);
}

static void
mixer_component_fixture_tear_down (MixerComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  v_assert_finalize_object (fixture->mixer);
  v_assert_finalize_object (fixture->input);
  v_assert_finalize_object (fixture->output);
}

static void
test_mixer_component_adapter (MixerComponentFixture *fixture,
                              gconstpointer          user_data)
{
  ValentMixerControl *adapter;
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  ValentMixerStream *stream;
  PeasPluginInfo *plugin_info;

  while ((adapter = valent_mock_mixer_control_get_instance ()) == NULL)
    continue;

  /* Properties */
  g_object_get (adapter,
                "plugin-info", &plugin_info,
                NULL);

  g_assert_nonnull (plugin_info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);

  /* Add Streams */
  g_signal_connect (adapter,
                    "stream-added::input",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_control_emit_stream_added (adapter, fixture->input);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  g_signal_connect (adapter,
                    "stream-added::output",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_control_emit_stream_added (adapter, fixture->output);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  /* Check Defaults */
  stream = valent_mixer_control_get_default_input (adapter);
  g_assert_true (stream == fixture->input);

  stream = valent_mixer_control_get_default_output (adapter);
  g_assert_true (stream == fixture->output);

  /* Check Lists */
  inputs = valent_mixer_control_get_inputs (adapter);
  g_assert_true (g_ptr_array_index (inputs, 0) == fixture->input);

  outputs = valent_mixer_control_get_outputs (adapter);
  g_assert_true (g_ptr_array_index (outputs, 0) == fixture->output);

  /* Remove Streams */
  g_signal_connect (adapter,
                    "stream-removed::input",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_control_emit_stream_removed (adapter, fixture->input);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  g_signal_connect (adapter,
                    "stream-removed::output",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_control_emit_stream_removed (adapter, fixture->output);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (adapter, fixture);
}

static void
test_mixer_component_stream (MixerComponentFixture *fixture,
                             gconstpointer          user_data)
{
  ValentMixerControl *adapter;
  ValentMixerStreamFlags flags;
  int level;
  gboolean muted;
  char *name, *description;

  while ((adapter = valent_mock_mixer_control_get_instance ()) == NULL)
    continue;

  /* Add Streams */
  g_signal_connect (adapter,
                    "stream-added::input",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_control_emit_stream_added (adapter, fixture->input);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  g_signal_connect (adapter,
                    "stream-added::output",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_control_emit_stream_added (adapter, fixture->output);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  /* Test Stream */
  g_object_get (fixture->input,
                "description", &description,
                "flags",       &flags,
                "level",       &level,
                "muted",       &muted,
                "name",        &name,
                NULL);

  g_assert_cmpuint (flags & VALENT_MIXER_STREAM_SOURCE, ==, VALENT_MIXER_STREAM_SOURCE);
  g_assert_cmpuint (level, ==, 50);
  g_assert_true (muted);
  g_assert_cmpstr (name, ==, "test_source");
  g_assert_cmpstr (description, ==, "Test Microphone");

  g_free (name);
  g_free (description);

  g_signal_connect (adapter,
                    "stream-changed",
                    G_CALLBACK (on_stream_changed),
                    fixture);

  valent_mixer_stream_set_level (fixture->output, 100);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  /* Remove Streams */
  g_signal_connect (adapter,
                    "stream-removed::input",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_control_emit_stream_removed (adapter, fixture->input);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  g_signal_connect (adapter,
                    "stream-removed::output",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_control_emit_stream_removed (adapter, fixture->output);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (adapter, fixture);
}

static void
test_mixer_component_self (MixerComponentFixture *fixture,
                           gconstpointer          user_data)
{
  ValentMixerControl *adapter;
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  ValentMixerStream *stream;

  while ((adapter = valent_mock_mixer_control_get_instance ()) == NULL)
    continue;

  /* Add Streams */
  g_signal_connect (fixture->mixer,
                    "stream-added::input",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_control_emit_stream_added (adapter, fixture->input);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_connect (fixture->mixer,
                    "stream-added::output",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_control_emit_stream_added (adapter, fixture->output);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_connect (fixture->mixer,
                    "stream-changed",
                    G_CALLBACK (on_stream_changed),
                    fixture);

  valent_mixer_stream_set_level (fixture->output, 100);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  /* Check Defaults */
  stream = valent_mixer_get_default_input (fixture->mixer);
  g_assert_true (stream == fixture->input);

  stream = valent_mixer_get_default_output (fixture->mixer);
  g_assert_true (stream == fixture->output);

  /* Check Lists */
  inputs = valent_mixer_get_inputs (fixture->mixer);
  g_assert_true (g_ptr_array_index (inputs, 0) == fixture->input);

  outputs = valent_mixer_get_outputs (fixture->mixer);
  g_assert_true (g_ptr_array_index (outputs, 0) == fixture->output);

  /* Remove Streams */
  g_signal_connect (fixture->mixer,
                    "stream-removed::input",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_control_emit_stream_removed (adapter, fixture->input);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_connect (fixture->mixer,
                    "stream-removed::output",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_control_emit_stream_removed (adapter, fixture->output);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->mixer, fixture);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/components/mixer/adapter",
              MixerComponentFixture, NULL,
              mixer_component_fixture_set_up,
              test_mixer_component_adapter,
              mixer_component_fixture_tear_down);

  g_test_add ("/components/mixer/stream",
              MixerComponentFixture, NULL,
              mixer_component_fixture_set_up,
              test_mixer_component_stream,
              mixer_component_fixture_tear_down);

  g_test_add ("/components/mixer/self",
              MixerComponentFixture, NULL,
              mixer_component_fixture_set_up,
              test_mixer_component_self,
              mixer_component_fixture_tear_down);

  return g_test_run ();
}
