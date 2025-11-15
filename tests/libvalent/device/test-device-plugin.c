// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  ValentDevice *device;
  GObject      *extension;
  JsonNode     *packets;
} DevicePluginFixture;

static void
device_fixture_set_up (DevicePluginFixture *fixture,
                       gconstpointer        user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  JsonNode *peer_identity;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (context, "plugin", "mock");

  fixture->packets = valent_test_load_json ("core.json");
  peer_identity = json_object_get_member (json_node_get_object (fixture->packets),
                                          "peer-identity");

  fixture->device = valent_device_new_full (peer_identity, NULL);
  fixture->extension = peas_engine_create_extension (engine,
                                                     plugin_info,
                                                     VALENT_TYPE_DEVICE_PLUGIN,
                                                     "iri",     "urn:valent:device:mock",
                                                     "parent",  fixture->device,
                                                     "context", context,
                                                     NULL);
}

static void
device_fixture_tear_down (DevicePluginFixture *fixture,
                          gconstpointer  user_data)
{
  v_await_finalize_object (fixture->device);
  v_await_finalize_object (fixture->extension);
  g_clear_pointer (&fixture->packets, json_node_unref);
}

static void
test_device_plugin_basic (DevicePluginFixture *fixture,
                          gconstpointer        user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) extension = NULL;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (context, "plugin", "mock");

  VALENT_TEST_CHECK ("Plugin can be constructed");
  extension = peas_engine_create_extension (engine,
                                            plugin_info,
                                            VALENT_TYPE_DEVICE_PLUGIN,
                                            "iri",     "urn:valent:device:mock",
                                            "parent",  NULL,
                                            "context", context,
                                            NULL);

  g_assert_true (VALENT_IS_DEVICE_PLUGIN (extension));
}

static void
on_activate (GSimpleAction *action,
             GVariant      *parameter,
             gboolean      *activated)
{
  if (activated)
    *activated = g_variant_get_boolean (parameter);
}

static void
test_device_plugin_actions (DevicePluginFixture *fixture,
                            gconstpointer        user_data)
{
  g_autoptr (GSimpleAction) action = NULL;
  gboolean has_action = FALSE;
  gboolean enabled = FALSE;
  const GVariantType *parameter_type = NULL;
  const GVariantType *state_type = NULL;
  GVariant *state_hint = NULL;
  GVariant *state = NULL;
  gboolean watch = FALSE;

  VALENT_TEST_CHECK ("Actions can be queried");
  has_action = g_action_group_query_action (G_ACTION_GROUP (fixture->extension),
                                            "state",
                                            &enabled,
                                            &parameter_type,
                                            &state_type,
                                            &state_hint,
                                            &state);
  g_assert_true (has_action);
  g_assert_true (enabled);
  g_assert_null (parameter_type);
  g_assert_true (g_variant_type_equal (state_type, G_VARIANT_TYPE_BOOLEAN));
  g_assert_null (state_hint);
  g_assert_true (g_variant_get_boolean (state));
  g_clear_pointer (&state, g_variant_unref);

  /* Signals */
  valent_test_watch_signal (fixture->extension, "action-added", &watch);
  valent_test_watch_signal (fixture->extension, "action-enabled-changed", &watch);
  valent_test_watch_signal (fixture->extension, "action-removed", &watch);
  valent_test_watch_signal (fixture->extension, "action-state-changed", &watch);

  VALENT_TEST_CHECK ("Stateful actions can be changed");
  g_action_group_change_action_state (G_ACTION_GROUP (fixture->extension),
                                      "state",
                                      g_variant_new_boolean (FALSE));
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("Stateful actions can be read");
  state = g_action_group_get_action_state (G_ACTION_GROUP (fixture->extension),
                                           "state");
  g_assert_false (g_variant_get_boolean (state));
  g_clear_pointer (&state, g_variant_unref);

  VALENT_TEST_CHECK ("Actions can be added");
  action = g_simple_action_new ("action", G_VARIANT_TYPE_BOOLEAN);
  g_action_map_add_action (G_ACTION_MAP (fixture->extension),
                           G_ACTION (action));
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("Actions can be disabled");
  g_simple_action_set_enabled (action, FALSE);
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("Actions can be enabled");
  g_simple_action_set_enabled (action, TRUE);
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("Actions can be activated");
  g_signal_connect (action,
                    "activate",
                    G_CALLBACK (on_activate),
                    &watch);
  g_action_group_activate_action (G_ACTION_GROUP (fixture->extension),
                                  "action",
                                  g_variant_new_boolean (TRUE));
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("Actions can be removed");
  g_action_map_remove_action (G_ACTION_MAP (fixture->extension), "action");
  valent_test_await_boolean (&watch);

  valent_test_watch_clear (fixture->extension, &watch);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/device/device-plugin/basic",
              DevicePluginFixture, NULL,
              device_fixture_set_up,
              test_device_plugin_basic,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device-plugin/actions",
              DevicePluginFixture, NULL,
              device_fixture_set_up,
              test_device_plugin_actions,
              device_fixture_tear_down);

  return g_test_run ();
}

