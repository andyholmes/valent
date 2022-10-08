// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "valent-device-private.h"


typedef struct
{
  ValentDevice  *device;
  PeasExtension *extension;
} DevicePluginFixture;

static void
device_fixture_set_up (DevicePluginFixture *fixture,
                       gconstpointer        user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");

  fixture->device = g_object_new (VALENT_TYPE_DEVICE,
                                  "id", "mock-device",
                                  NULL);
  fixture->extension = peas_engine_create_extension (engine,
                                                     plugin_info,
                                                     VALENT_TYPE_DEVICE_PLUGIN,
                                                     "device", fixture->device,
                                                     NULL);
}

static void
device_fixture_tear_down (DevicePluginFixture *fixture,
                          gconstpointer  user_data)
{
  v_await_finalize_object (fixture->device);
  v_await_finalize_object (fixture->extension);
}

static void
test_device_plugin_basic (DevicePluginFixture *fixture,
                          gconstpointer        user_data)
{
  g_autoptr (ValentDevice) device = NULL;
  PeasPluginInfo *plugin_info = NULL;
  GStrv capabilities = NULL;

  /* Test properties */
  g_object_get (fixture->extension,
                "device",      &device,
                "plugin-info", &plugin_info,
                NULL);

  g_assert_true (VALENT_IS_DEVICE (device));
  g_assert_nonnull (plugin_info);

  /* Capabilities */
  capabilities = valent_device_plugin_get_incoming (plugin_info);
  g_assert_cmpuint (g_strv_length (capabilities), ==, 2);
  g_clear_pointer (&capabilities, g_strfreev);

  capabilities = valent_device_plugin_get_outgoing (plugin_info);
  g_assert_cmpuint (g_strv_length (capabilities), ==, 2);
  g_clear_pointer (&capabilities, g_strfreev);

  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);
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
on_action_added (GActionGroup *action_group,
                 const char   *action_name,
                 gboolean     *emitted)
{
  g_assert_cmpstr (action_name, ==, "action");

  if (emitted)
    *emitted = TRUE;
}

static void
on_action_enabled_changed (GActionGroup *action_group,
                           const char   *action_name,
                           gboolean      enabled,
                           gboolean     *emitted)
{
  g_assert_cmpstr (action_name, ==, "action");

  if (emitted)
    *emitted = TRUE;
}

static void
on_action_removed (GActionGroup *action_group,
                   const char   *action_name,
                   gboolean     *emitted)
{
  g_assert_cmpstr (action_name, ==, "action");

  if (emitted)
    *emitted = TRUE;
}

static void
on_action_state_changed (GActionGroup *action_group,
                         const char   *action_name,
                         GVariant     *value,
                         gboolean     *emitted)
{
  g_assert_cmpstr (action_name, ==, "state");

  if (emitted)
    *emitted = TRUE;
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
  gboolean emitted = FALSE;

  valent_device_plugin_enable (VALENT_DEVICE_PLUGIN (fixture->extension));

  g_signal_connect (fixture->extension,
                    "action-added",
                    G_CALLBACK (on_action_added),
                    &emitted);
  g_signal_connect (fixture->extension,
                    "action-enabled-changed",
                    G_CALLBACK (on_action_enabled_changed),
                    &emitted);
  g_signal_connect (fixture->extension,
                    "action-removed",
                    G_CALLBACK (on_action_removed),
                    &emitted);
  g_signal_connect (fixture->extension,
                    "action-state-changed",
                    G_CALLBACK (on_action_state_changed),
                    &emitted);

  /* Query */
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

  /* Change State */
  g_action_group_change_action_state (G_ACTION_GROUP (fixture->extension),
                                      "state",
                                      g_variant_new_boolean (FALSE));
  g_assert_true (emitted);
  emitted = FALSE;

  state = g_action_group_get_action_state (G_ACTION_GROUP (fixture->extension),
                                           "state");
  g_assert_false (g_variant_get_boolean (state));
  g_clear_pointer (&state, g_variant_unref);

  /* Add */
  action = g_simple_action_new ("action", G_VARIANT_TYPE_BOOLEAN);
  g_action_map_add_action (G_ACTION_MAP (fixture->extension),
                           G_ACTION (action));
  g_assert_true (emitted);
  emitted = FALSE;

  /* Enable/Disable */
  g_simple_action_set_enabled (action, FALSE);
  g_assert_true (emitted);
  emitted = FALSE;

  g_simple_action_set_enabled (action, TRUE);
  g_assert_true (emitted);
  emitted = FALSE;

  /* Activate */
  g_signal_connect (action,
                    "activate",
                    G_CALLBACK (on_activate),
                    &emitted);
  g_action_group_activate_action (G_ACTION_GROUP (fixture->extension),
                                  "action",
                                  g_variant_new_boolean (TRUE));
  g_assert_true (emitted);
  emitted = FALSE;

  /* Remove */
  g_action_map_remove_action (G_ACTION_MAP (fixture->extension), "action");
  g_assert_true (emitted);
  emitted = FALSE;

  g_signal_handlers_disconnect_by_data (fixture->extension, &emitted);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/core/device-plugin/basic",
              DevicePluginFixture, NULL,
              device_fixture_set_up,
              test_device_plugin_basic,
              device_fixture_tear_down);

  g_test_add ("/libvalent/core/device-plugin/actions",
              DevicePluginFixture, NULL,
              device_fixture_set_up,
              test_device_plugin_actions,
              device_fixture_tear_down);

  return g_test_run ();
}

