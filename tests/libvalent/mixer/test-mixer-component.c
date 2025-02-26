// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
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
test_mixer_component_stream (MixerComponentFixture *fixture,
                             gconstpointer          user_data)
{
  ValentMixerStream *stream;
  ValentMixerDirection direction;
  unsigned int level;
  gboolean muted;
  g_autofree char *name = NULL;
  g_autofree char *description = NULL;

  stream = g_object_new (VALENT_TYPE_MIXER_STREAM,
                         "name",        "test.output",
                         "description", "Test Speakers",
                         "direction",   VALENT_MIXER_OUTPUT,
                         "level",       50,
                         "muted",       TRUE,
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (stream,
                "description", &description,
                "direction",   &direction,
                "level",       &level,
                "muted",       &muted,
                "name",        &name,
                NULL);

  g_assert_cmpuint (direction, ==, VALENT_MIXER_OUTPUT);
  g_assert_cmpuint (level, ==, 50);
  g_assert_true (muted);
  g_assert_cmpstr (name, ==, "test.output");
  g_assert_cmpstr (description, ==, "Test Speakers");

  g_object_set (stream,
                "level", 100,
                "muted", FALSE,
                NULL);

  g_assert_cmpuint (valent_mixer_stream_get_direction (stream), ==, VALENT_MIXER_OUTPUT);
  g_assert_cmpuint (valent_mixer_stream_get_level (stream), ==, 100);
  g_assert_false (valent_mixer_stream_get_muted (stream));
  g_assert_cmpstr (name, ==, "test.output");
  g_assert_cmpstr (description, ==, "Test Speakers");

  v_await_finalize_object (stream);
}

static void
mixer_component_fixture_tear_down (MixerComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  v_await_finalize_object (fixture->mixer);
  v_await_finalize_object (fixture->adapter);
  v_await_finalize_object (fixture->input1);
  v_await_finalize_object (fixture->input2);
  v_await_finalize_object (fixture->output1);
  v_await_finalize_object (fixture->output2);
}

static void
test_mixer_component_adapter (MixerComponentFixture *fixture,
                              gconstpointer          user_data)
{
  GListModel *list = G_LIST_MODEL (fixture->adapter);
  unsigned int n_items = 0;
  ValentMixerStream *input_out = NULL;
  ValentMixerStream *output_out = NULL;

  valent_mixer_adapter_stream_added (fixture->adapter, fixture->input1);
  valent_mixer_adapter_stream_added (fixture->adapter, fixture->output1);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->adapter)), ==, 2);

  VALENT_TEST_CHECK ("Adapter implements GListModel correctly");
  g_assert_true (G_IS_LIST_MODEL (list));
  g_assert_cmpuint (g_list_model_get_n_items (list), >, 0);
  g_assert_true (g_list_model_get_item_type (list) == VALENT_TYPE_MIXER_STREAM);

  n_items = g_list_model_get_n_items (list);
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = NULL;

      item = g_list_model_get_item (list, i);
      g_assert_true (VALENT_IS_MIXER_STREAM (item));
    }

  valent_mixer_adapter_stream_added (fixture->adapter, fixture->input2);
  valent_mixer_adapter_stream_added (fixture->adapter, fixture->output2);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->adapter)), ==, 4);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->adapter,
                "default-input",  &input_out,
                "default-output", &output_out,
                NULL);
  g_assert_true (input_out == fixture->input1);
  g_clear_object (&input_out);
  g_assert_true (output_out == fixture->output1);
  g_clear_object (&output_out);

  g_object_set (fixture->adapter,
                "default-input",  fixture->input2,
                "default-output", fixture->output2,
                NULL);
  g_object_get (fixture->adapter,
                "default-input",  &input_out,
                "default-output", &output_out,
                NULL);
  g_assert_true (input_out == fixture->input2);
  g_clear_object (&input_out);
  g_assert_true (output_out == fixture->output2);
  g_clear_object (&output_out);

  /* Remove Streams */
  valent_mixer_adapter_stream_removed (fixture->adapter, fixture->input2);
  valent_mixer_adapter_stream_removed (fixture->adapter, fixture->output2);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->adapter)), ==, 2);
}

static void
test_mixer_component_self (MixerComponentFixture *fixture,
                           gconstpointer          user_data)
{
  ValentMixer *mixer = valent_mixer_get_default ();
  ValentMixerStream *input_out = NULL;
  ValentMixerStream *output_out = NULL;
  unsigned int n_items = 0;

  VALENT_TEST_CHECK ("Component implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (mixer));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (mixer)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (mixer)) == VALENT_TYPE_MIXER_ADAPTER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (mixer));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (mixer), i);
      g_assert_true (VALENT_IS_MIXER_ADAPTER (item));
    }

  g_signal_handlers_disconnect_by_data (fixture->mixer, fixture);

  valent_mixer_adapter_stream_added (fixture->adapter, fixture->input1);
  valent_mixer_adapter_stream_added (fixture->adapter, fixture->input2);
  valent_mixer_adapter_stream_added (fixture->adapter, fixture->output1);
  valent_mixer_adapter_stream_added (fixture->adapter, fixture->output2);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->mixer,
                "default-input",  &input_out,
                "default-output", &output_out,
                NULL);
  g_assert_true (input_out == fixture->input1);
  g_clear_object (&input_out);
  g_assert_true (output_out == fixture->output1);
  g_clear_object (&output_out);

  g_object_set (fixture->mixer,
                "default-input",  fixture->input2,
                "default-output", fixture->output2,
                NULL);
  g_assert_true (valent_mixer_get_default_input (mixer) == fixture->input2);
  g_assert_true (valent_mixer_get_default_output (mixer) == fixture->output2);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/mixer/stream",
              MixerComponentFixture, NULL,
              mixer_component_fixture_set_up,
              test_mixer_component_stream,
              mixer_component_fixture_tear_down);

  g_test_add ("/libvalent/mixer/adapter",
              MixerComponentFixture, NULL,
              mixer_component_fixture_set_up,
              test_mixer_component_adapter,
              mixer_component_fixture_tear_down);

  g_test_add ("/libvalent/mixer/self",
              MixerComponentFixture, NULL,
              mixer_component_fixture_set_up,
              test_mixer_component_self,
              mixer_component_fixture_tear_down);

  return g_test_run ();
}

