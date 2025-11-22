// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#define _valent_test_event_cmpstr(obj, str)                                   \
  G_STMT_START {                                                              \
    GVariant *_value = g_action_group_get_action_state (G_ACTION_GROUP (obj), \
                                                        "event");             \
    g_assert_cmpstr (g_variant_get_string (_value, NULL), ==, str);           \
    g_variant_unref (_value);                                                 \
  } G_STMT_END


typedef struct
{
} InputComponentFixture;

static void
input_component_fixture_set_up (InputComponentFixture *fixture,
                                gconstpointer          user_data)
{
}

static void
input_component_fixture_tear_down (InputComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  v_await_finalize_object (valent_input_get_default ());
}

static void
test_input_component_adapter (InputComponentFixture *fixture,
                              gconstpointer          user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) adapter = NULL;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_INPUT_ADAPTER,
                                          "iri",     "urn:valent:input:mock",
                                          "parent",  NULL,
                                          "context", context,
                                          NULL);

  VALENT_TEST_CHECK ("Adapter emulates pointer motion (relative)");
  valent_input_adapter_pointer_motion (VALENT_INPUT_ADAPTER (adapter), 1.0, 1.0);
  _valent_test_event_cmpstr (VALENT_INPUT_ADAPTER (adapter), "POINTER MOTION 1.0 1.0");

  VALENT_TEST_CHECK ("Adapter emulates pointer scrolling");
  valent_input_adapter_pointer_axis (VALENT_INPUT_ADAPTER (adapter), 0.0, 1.0);
  _valent_test_event_cmpstr (VALENT_INPUT_ADAPTER (adapter), "POINTER AXIS 0.0 1.0");

  VALENT_TEST_CHECK ("Adapter emulates pointer button press");
  valent_input_adapter_pointer_button (VALENT_INPUT_ADAPTER (adapter), VALENT_POINTER_PRIMARY, TRUE);
  _valent_test_event_cmpstr (VALENT_INPUT_ADAPTER (adapter), "POINTER BUTTON 1 1");

  VALENT_TEST_CHECK ("Adapter emulates pointer button release");
  valent_input_adapter_pointer_button (VALENT_INPUT_ADAPTER (adapter), VALENT_POINTER_PRIMARY, FALSE);
  _valent_test_event_cmpstr (VALENT_INPUT_ADAPTER (adapter), "POINTER BUTTON 1 0");

  VALENT_TEST_CHECK ("Adapter emulates keysym press");
  valent_input_adapter_keyboard_keysym (VALENT_INPUT_ADAPTER (adapter), 'a', TRUE);
  _valent_test_event_cmpstr (VALENT_INPUT_ADAPTER (adapter), "KEYSYM 97 1");

  VALENT_TEST_CHECK ("Adapter emulates keysym release");
  valent_input_adapter_keyboard_keysym (VALENT_INPUT_ADAPTER (adapter), 'a', FALSE);
  _valent_test_event_cmpstr (VALENT_INPUT_ADAPTER (adapter), "KEYSYM 97 0");
}

static void
test_input_component_self (InputComponentFixture *fixture,
                           gconstpointer          user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  ValentInput *input = valent_input_get_default ();
  g_autoptr (GObject) adapter = NULL;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  VALENT_TEST_CHECK ("Component implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (input));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (input)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (input)) == VALENT_TYPE_INPUT_ADAPTER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (input));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (input), i);
      g_assert_true (VALENT_IS_INPUT_ADAPTER (item));
    }

  VALENT_TEST_CHECK ("Component can export adapters");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_INPUT_ADAPTER,
                                          "iri",     "urn:valent:input:remote",
                                          "parent",  NULL,
                                          "context", context,
                                          NULL);

  valent_component_export_adapter (VALENT_COMPONENT (input),
                                   VALENT_EXTENSION (adapter));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (input)), ==, n_items + 1);

  VALENT_TEST_CHECK ("Component can unexport adapters");
  valent_component_unexport_adapter (VALENT_COMPONENT (input),
                                     VALENT_EXTENSION (adapter));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (input)), ==, n_items);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/input/adapter",
              InputComponentFixture, NULL,
              input_component_fixture_set_up,
              test_input_component_adapter,
              input_component_fixture_tear_down);

  g_test_add ("/libvalent/input/self",
              InputComponentFixture, NULL,
              input_component_fixture_set_up,
              test_input_component_self,
              input_component_fixture_tear_down);

  return g_test_run ();
}
