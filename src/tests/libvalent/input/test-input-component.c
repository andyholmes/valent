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
  v_assert_finalize_object (fixture->input);
}

static void
test_input_component_adapter (InputComponentFixture *fixture,
                              gconstpointer          user_data)
{

  ValentInputAdapter *adapter;
  PeasPluginInfo *plugin_info;

  while ((adapter = valent_mock_input_adapter_get_instance ()) == NULL)
    continue;

  /* Properties */
  g_object_get (adapter,
                "plugin-info", &plugin_info,
                NULL);

  g_assert_nonnull (plugin_info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);

  /* Pointer Motion (relative) */
  valent_input_adapter_pointer_motion (adapter, 1.0, 1.0);
  valent_test_event_cmpstr ("POINTER MOTION 1.0 1.0");

  /* Pointer Scroll */
  valent_input_adapter_pointer_axis (adapter, 0.0, 1.0);
  valent_test_event_cmpstr ("POINTER AXIS 0.0 1.0");

  /* Pointer Button (press/release) */
  valent_input_adapter_pointer_button (adapter, VALENT_POINTER_PRIMARY, TRUE);
  valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  valent_input_adapter_pointer_button (adapter, VALENT_POINTER_PRIMARY, FALSE);
  valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  /* Keysym (press/release) */
  valent_input_adapter_keyboard_keysym (adapter, 'a', TRUE);
  valent_test_event_cmpstr ("KEYSYM 97 1");
  valent_input_adapter_keyboard_keysym (adapter, 'a', FALSE);
  valent_test_event_cmpstr ("KEYSYM 97 0");
}

static void
test_input_component_self (InputComponentFixture *fixture,
                            gconstpointer          user_data)
{
  /* Pointer Motion (relative) */
  valent_input_pointer_motion (fixture->input, 1.0, 1.0);
  valent_test_event_cmpstr ("POINTER MOTION 1.0 1.0");

  /* Pointer Scroll */
  valent_input_pointer_axis (fixture->input, 0.0, 1.0);
  valent_test_event_cmpstr ("POINTER AXIS 0.0 1.0");

  /* Pointer Button (press/release) */
  valent_input_pointer_button (fixture->input, VALENT_POINTER_PRIMARY, TRUE);
  valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  valent_input_pointer_button (fixture->input, VALENT_POINTER_PRIMARY, FALSE);
  valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  /* Keysym (press/release) */
  valent_input_keyboard_keysym (fixture->input, 'a', TRUE);
  valent_test_event_cmpstr ("KEYSYM 97 1");
  valent_input_keyboard_keysym (fixture->input, 'a', FALSE);
  valent_test_event_cmpstr ("KEYSYM 97 0");
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/components/input/adapter",
              InputComponentFixture, NULL,
              input_component_fixture_set_up,
              test_input_component_adapter,
              input_component_fixture_tear_down);

  g_test_add ("/components/input/self",
              InputComponentFixture, NULL,
              input_component_fixture_set_up,
              test_input_component_self,
              input_component_fixture_tear_down);

  return g_test_run ();
}
