// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-input.h>
#include <libvalent-test.h>


typedef struct
{
  ValentInput *input;
} InputComponentFixture;

static void
input_component_fixture_set_up (InputComponentFixture *fixture,
                                gconstpointer          user_data)
{
  fixture->input = valent_input_get_default ();
}

static void
input_component_fixture_tear_down (InputComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  g_assert_finalize_object (fixture->input);
}

static void
test_input_component_basic (InputComponentFixture *fixture,
                            gconstpointer          user_data)
{
  /* Pointer Motion (relative) */
  valent_input_pointer_motion (fixture->input, 1.0, 1.0);
  valent_test_event_cmpstr ("POINTER MOTION 1.0 1.0");

  /* Pointer Motion (absolute) */
  valent_input_pointer_position (fixture->input, 0.0, 0.0);
  valent_test_event_cmpstr ("POINTER POSITION 0.0 0.0");

  /* Pointer Scroll */
  valent_input_pointer_axis (fixture->input, 0.0, 1.0);
  valent_test_event_cmpstr ("POINTER AXIS 0.0 1.0");

  /* Pointer Button (press/release) */
  valent_input_pointer_button (fixture->input, VALENT_POINTER_PRIMARY, TRUE);
  valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  valent_input_pointer_button (fixture->input, VALENT_POINTER_PRIMARY, FALSE);
  valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  valent_input_pointer_click (fixture->input, VALENT_POINTER_PRIMARY);
  valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  /* Keysym (press/release) */
  valent_input_keyboard_keysym (fixture->input, 'a', TRUE);
  valent_test_event_cmpstr ("KEYSYM 97 1");
  valent_input_keyboard_keysym (fixture->input, 'a', FALSE);
  valent_test_event_cmpstr ("KEYSYM 97 0");
}

static void
test_input_component_dispose (InputComponentFixture *fixture,
                              gconstpointer          user_data)
{
  GPtrArray *extensions;
  PeasEngine *engine;
  g_autoptr (GSettings) settings = NULL;

  /* Add a store to the provider */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->input));
  g_assert_cmpuint (extensions->len, ==, 1);
  g_ptr_array_unref (extensions);

  /* Wait for provider to resolve */
  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Disable/Enable the provider */
  settings = valent_component_new_settings ("input", "mock");

  g_settings_set_boolean (settings, "enabled", FALSE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->input));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);

  g_settings_set_boolean (settings, "enabled", TRUE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->input));
  g_assert_cmpuint (extensions->len, ==, 1);
  g_ptr_array_unref (extensions);

  /* Unload the provider */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->input));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/components/input/basic",
              InputComponentFixture, NULL,
              input_component_fixture_set_up,
              test_input_component_basic,
              input_component_fixture_tear_down);

  g_test_add ("/components/input/dispose",
              InputComponentFixture, NULL,
              input_component_fixture_set_up,
              test_input_component_dispose,
              input_component_fixture_tear_down);

  return g_test_run ();
}
