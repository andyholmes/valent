// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-mixer.h>
#include <libvalent-test.h>


typedef struct
{
  ValentMixer        *mixer;
  ValentMixerAdapter *adapter;
  ValentMixerStream  *input1;
  ValentMixerStream  *input2;
  ValentMixerStream  *output1;
  ValentMixerStream  *output2;
  gpointer            data;
} MixerComponentFixture;

static void
on_default_input_changed (GObject               *object,
                          GParamSpec            *pspec,
                          MixerComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_default_output_changed (GObject               *object,
                           GParamSpec            *pspec,
                           MixerComponentFixture *fixture)
{
  fixture->data = object;
}

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
  fixture->adapter = valent_test_await_adapter (fixture->mixer);
  fixture->input1 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                  "name",        "test_source1",
                                  "description", "Test Microphone",
                                  "direction",   VALENT_MIXER_INPUT,
                                  "level",       50,
                                  "muted",       TRUE,
                                  NULL);
  fixture->input2 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                  "name",        "test_source2",
                                  "description", "Test Headset",
                                  "direction",   VALENT_MIXER_INPUT,
                                  "level",       50,
                                  "muted",       TRUE,
                                  NULL);
  fixture->output1 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                   "name",        "test_sink1",
                                   "description", "Test Speakers",
                                   "direction",   VALENT_MIXER_OUTPUT,
                                   NULL);
  fixture->output2 = g_object_new (VALENT_TYPE_MIXER_STREAM,
                                   "name",        "test_sink2",
                                   "description", "Test Headphones",
                                   "direction",   VALENT_MIXER_OUTPUT,
                                   NULL);

  g_object_ref (fixture->adapter);
}

static void
mixer_component_fixture_tear_down (MixerComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  v_assert_finalize_object (fixture->mixer);
  v_await_finalize_object (fixture->adapter);
  v_assert_finalize_object (fixture->input1);
  v_assert_finalize_object (fixture->input2);
  v_assert_finalize_object (fixture->output1);
  v_assert_finalize_object (fixture->output2);
}

static void
test_mixer_component_adapter (MixerComponentFixture *fixture,
                              gconstpointer          user_data)
{
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  ValentMixerStream *stream;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentMixerStream) default_input = NULL;
  g_autoptr (ValentMixerStream) default_output = NULL;

  /* Properties */
  g_object_get (fixture->adapter,
                "default-input",  &default_input,
                "default-output", &default_output,
                "plugin-info",    &plugin_info,
                NULL);

  g_assert_null (default_input);
  g_assert_null (default_output);
  g_assert_nonnull (plugin_info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);

  /* Add Streams */
  g_signal_connect (fixture->adapter,
                    "stream-added::input",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->input1);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->input2);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  g_signal_connect (fixture->adapter,
                    "stream-added::output",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->output1);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->output2);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  /* Check Defaults */
  g_signal_connect (fixture->adapter,
                    "notify::default-input",
                    G_CALLBACK (on_default_input_changed),
                    fixture);
  g_signal_connect (fixture->adapter,
                    "notify::default-output",
                    G_CALLBACK (on_default_output_changed),
                    fixture);

  stream = valent_mixer_adapter_get_default_input (fixture->adapter);
  g_assert_true (stream == fixture->input1);

  valent_mixer_adapter_set_default_input (fixture->adapter, fixture->input2);
  g_assert_true (fixture->data == fixture->adapter);
  stream = valent_mixer_adapter_get_default_input (fixture->adapter);
  g_assert_true (stream == fixture->input2);

  stream = valent_mixer_adapter_get_default_output (fixture->adapter);
  g_assert_true (stream == fixture->output1);

  valent_mixer_adapter_set_default_output (fixture->adapter, fixture->output2);
  g_assert_true (fixture->data == fixture->adapter);
  stream = valent_mixer_adapter_get_default_output (fixture->adapter);
  g_assert_true (stream == fixture->output2);

  g_object_set (fixture->adapter,
                "default-input",  fixture->input1,
                "default-output", fixture->output1,
                NULL);

  stream = valent_mixer_adapter_get_default_input (fixture->adapter);
  g_assert_true (stream == fixture->input1);
  stream = valent_mixer_adapter_get_default_output (fixture->adapter);
  g_assert_true (stream == fixture->output1);

  /* Check Lists */
  inputs = valent_mixer_adapter_get_inputs (fixture->adapter);
  g_assert_true (g_ptr_array_index (inputs, 0) == fixture->input1);
  g_assert_true (g_ptr_array_index (inputs, 1) == fixture->input2);
  g_clear_pointer (&inputs, g_ptr_array_unref);

  outputs = valent_mixer_adapter_get_outputs (fixture->adapter);
  g_assert_true (g_ptr_array_index (outputs, 0) == fixture->output1);
  g_assert_true (g_ptr_array_index (outputs, 1) == fixture->output2);
  g_clear_pointer (&outputs, g_ptr_array_unref);

  /* Remove Streams */
  g_signal_connect (fixture->adapter,
                    "stream-removed::input",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_adapter_emit_stream_removed (fixture->adapter, fixture->input2);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  inputs = valent_mixer_adapter_get_inputs (fixture->adapter);
  g_assert_cmpuint (inputs->len, ==, 1);
  g_clear_pointer (&inputs, g_ptr_array_unref);

  g_signal_connect (fixture->adapter,
                    "stream-removed::output",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_adapter_emit_stream_removed (fixture->adapter, fixture->output2);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  outputs = valent_mixer_adapter_get_outputs (fixture->adapter);
  g_assert_cmpuint (outputs->len, ==, 1);
  g_clear_pointer (&outputs, g_ptr_array_unref);

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_mixer_component_stream (MixerComponentFixture *fixture,
                             gconstpointer          user_data)
{
  ValentMixerDirection direction;
  int level;
  gboolean muted;
  char *name, *description;

  /* Add Streams */
  g_signal_connect (fixture->adapter,
                    "stream-added::input",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->input1);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  /* Test Stream */
  g_object_get (fixture->input1,
                "description", &description,
                "direction",   &direction,
                "level",       &level,
                "muted",       &muted,
                "name",        &name,
                NULL);

  g_assert_cmpuint (direction, ==, VALENT_MIXER_INPUT);
  g_assert_cmpuint (level, ==, 50);
  g_assert_true (muted);
  g_assert_cmpstr (name, ==, "test_source1");
  g_assert_cmpstr (description, ==, "Test Microphone");

  g_free (name);
  g_free (description);

  g_signal_connect (fixture->adapter,
                    "stream-changed::input",
                    G_CALLBACK (on_stream_changed),
                    fixture);

  g_object_set (fixture->input1,
                "level", 100,
                "muted", FALSE,
                NULL);

  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_mixer_component_self (MixerComponentFixture *fixture,
                           gconstpointer          user_data)
{
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  ValentMixerStream *stream;

  /* Add Streams */
  g_signal_connect (fixture->mixer,
                    "stream-added::input",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->input1);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->input2);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_connect (fixture->mixer,
                    "stream-added::output",
                    G_CALLBACK (on_stream_added),
                    fixture);

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->output1);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  valent_mixer_adapter_emit_stream_added (fixture->adapter, fixture->output2);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_connect (fixture->mixer,
                    "stream-changed",
                    G_CALLBACK (on_stream_changed),
                    fixture);

  valent_mixer_stream_set_level (fixture->output1, 100);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  /* Check Defaults */
  g_signal_connect (fixture->mixer,
                    "notify::default-input",
                    G_CALLBACK (on_default_input_changed),
                    fixture);
  g_signal_connect (fixture->mixer,
                    "notify::default-output",
                    G_CALLBACK (on_default_output_changed),
                    fixture);

  stream = valent_mixer_get_default_input (fixture->mixer);
  g_assert_true (stream == fixture->input1);

  valent_mixer_set_default_input (fixture->mixer, fixture->input2);
  g_assert_true (fixture->data == fixture->mixer);
  stream = valent_mixer_get_default_input (fixture->mixer);
  g_assert_true (stream == fixture->input2);

  stream = valent_mixer_get_default_output (fixture->mixer);
  g_assert_true (stream == fixture->output1);

  valent_mixer_set_default_output (fixture->mixer, fixture->output2);
  g_assert_true (fixture->data == fixture->mixer);
  stream = valent_mixer_get_default_output (fixture->mixer);
  g_assert_true (stream == fixture->output2);

  g_object_set (fixture->mixer,
                "default-input",  fixture->input1,
                "default-output", fixture->output1,
                NULL);

  g_object_get (fixture->mixer, "default-input", &stream, NULL);
  g_assert_true (stream == fixture->input1);
  g_clear_object (&stream);

  g_object_get (fixture->mixer, "default-output", &stream, NULL);
  g_assert_true (stream == fixture->output1);
  g_clear_object (&stream);

  /* Check Lists */
  inputs = valent_mixer_get_inputs (fixture->mixer);
  g_assert_true (g_ptr_array_index (inputs, 0) == fixture->input1);
  g_assert_true (g_ptr_array_index (inputs, 1) == fixture->input2);
  g_clear_pointer (&inputs, g_ptr_array_unref);

  outputs = valent_mixer_get_outputs (fixture->mixer);
  g_assert_true (g_ptr_array_index (outputs, 0) == fixture->output1);
  g_assert_true (g_ptr_array_index (outputs, 1) == fixture->output2);
  g_clear_pointer (&outputs, g_ptr_array_unref);

  /* Remove Streams */
  g_signal_connect (fixture->mixer,
                    "stream-removed::input",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_adapter_emit_stream_removed (fixture->adapter, fixture->input1);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_connect (fixture->mixer,
                    "stream-removed::output",
                    G_CALLBACK (on_stream_removed),
                    fixture);

  valent_mixer_adapter_emit_stream_removed (fixture->adapter, fixture->output1);
  g_assert_true (fixture->data == fixture->mixer);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->mixer, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

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
