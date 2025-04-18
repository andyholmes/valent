// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <math.h>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  ValentDevice  *device;
  ValentChannel *channel;
  ValentChannel *endpoint;
  JsonNode      *packets;
} DeviceFixture;


static inline JsonNode *
get_packet (DeviceFixture *fixture,
            const char    *name)
{
  return json_object_get_member (json_node_get_object (fixture->packets), name);
}

static JsonNode *
create_pair_packet (gboolean pair)
{
  g_autoptr (JsonBuilder) builder = NULL;

  valent_packet_init (&builder, "kdeconnect.pair");
  json_builder_set_member_name (builder, "pair");
  json_builder_add_boolean_value (builder, pair);

  if (pair)
    {
      int64_t timestamp = (int64_t)floor (valent_timestamp_ms () / 1000);

      json_builder_set_member_name (builder, "timestamp");
      json_builder_add_int_value (builder, timestamp);
    }

  return valent_packet_end (&builder);
}

static inline gboolean
valent_device_get_connected (ValentDevice *device)
{
  return (valent_device_get_state (device) & VALENT_DEVICE_STATE_CONNECTED) != 0;
}

static inline gboolean
valent_device_get_paired (ValentDevice *device)
{
  return (valent_device_get_state (device) & VALENT_DEVICE_STATE_PAIRED) != 0;
}

static void
device_fixture_set_up (DeviceFixture *fixture,
                       gconstpointer  user_data)
{
  JsonNode *identity, *peer_identity;

  fixture->packets = valent_test_load_json ("core.json");
  identity = get_packet (fixture, "identity");
  peer_identity = get_packet (fixture, "peer-identity");

  /* Init channels & device */
  valent_test_channel_pair (identity,
                            peer_identity,
                            &fixture->channel,
                            &fixture->endpoint);
  fixture->device = valent_device_new_full (peer_identity, NULL);
}

static void
device_fixture_tear_down (DeviceFixture *fixture,
                          gconstpointer  user_data)
{
  valent_channel_close (fixture->endpoint, NULL, NULL);
  v_await_finalize_object (fixture->endpoint);
  v_await_finalize_object (fixture->device);
  v_await_finalize_object (fixture->channel);

  g_clear_pointer (&fixture->packets, json_node_unref);
}

/*
 * Packet Helpers
 */
static void
endpoint_expect_packet_cb (ValentChannel  *channel,
                           GAsyncResult   *result,
                           JsonNode      **packet)
{
  g_autoptr (GError) error = NULL;

  *packet = valent_channel_read_packet_finish (channel, result, &error);
  g_assert_no_error (error);
}

static void
endpoint_expect_packet_pair (DeviceFixture *fixture,
                             gboolean       pair)
{
  g_autoptr (JsonNode) packet = NULL;

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)endpoint_expect_packet_cb,
                              &packet);
  valent_test_await_pointer (&packet);

  v_assert_packet_type (packet, "kdeconnect.pair");
  v_assert_packet_field (packet, "pair");

  if (pair)
    v_assert_packet_true (packet, "pair");
  else
    v_assert_packet_false (packet, "pair");
}

static void
endpoint_expect_packet_echo (DeviceFixture *fixture,
                             JsonNode      *packet)
{
  g_autoptr (JsonNode) echo = NULL;

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)endpoint_expect_packet_cb,
                              &echo);
  valent_test_await_pointer (&echo);

  v_assert_packet_type (echo, "kdeconnect.mock.echo");
  v_assert_packet_field (echo, "foo");
  v_assert_packet_cmpstr (echo, "foo", ==, "bar");
}

static void
endpoint_send_packet (DeviceFixture *fixture,
                      JsonNode      *packet)
{
  g_assert (fixture != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  valent_channel_write_packet (fixture->endpoint, packet, NULL, NULL, NULL);
  valent_test_await_signal (fixture->device, "notify::state");
}


/*
 * First test constructing a device before using the fixture
 */
static void
test_device_new (void)
{
  ValentDevice *device = NULL;
  g_autofree char *device_id = NULL;
  g_autofree char *icon_name = NULL;
  g_autofree char *id = NULL;
  g_autofree char *name = NULL;
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;

  GMenuModel *menu;

  device_id = valent_device_generate_id ();
  device = valent_device_new (device_id);
  g_assert_true (VALENT_IS_DEVICE (device));

  g_object_get (device,
                "id",        &id,
                "icon-name", &icon_name,
                "name",      &name,
                "state",     &state,
                NULL);

  /* id should be set, but everything else should be %FALSE or %NULL */
  g_assert_cmpstr (id, ==, device_id);
  g_assert_null (icon_name);
  g_assert_null (name);
  g_assert_cmpuint (state, ==, VALENT_DEVICE_STATE_NONE);

  menu = valent_device_get_menu (device);
  g_assert_true (G_IS_MENU (menu));

  v_await_finalize_object (device);
}

/*
 * Now test handling an identity packet with the fixture
 */
static void
test_device_basic (DeviceFixture *fixture,
                   gconstpointer  user_data)
{
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *id = NULL;
  g_autofree char *name = NULL;
  g_autofree char *icon_name = NULL;
  ValentDeviceState state = VALENT_DEVICE_STATE_NONE;

  /* Test properties */
  g_object_get (fixture->device,
                "context",          &context,
                "id",               &id,
                "name",             &name,
                "icon-name",        &icon_name,
                "state",            &state,
                NULL);

  g_assert_true (VALENT_IS_CONTEXT (context));
  g_assert_true (valent_device_validate_id (id));
  g_assert_cmpstr (valent_device_get_id (fixture->device), ==, id);
  g_assert_cmpstr (name, ==, "Test Device");
  g_assert_cmpstr (valent_device_get_name (fixture->device), ==, "Test Device");
  g_assert_cmpstr (icon_name, ==, "phone-symbolic");
  g_assert_cmpstr (valent_device_get_icon_name (fixture->device), ==, "phone-symbolic");
  g_assert_cmpuint (state, ==, VALENT_DEVICE_STATE_NONE);
  g_assert_cmpuint (valent_device_get_state (fixture->device), ==, VALENT_DEVICE_STATE_NONE);
}

static void
test_device_connecting (DeviceFixture *fixture,
                        gconstpointer  user_data)
{
  g_autoptr (ValentChannel) channel = NULL;

  /* Connect */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  channel = valent_device_ref_channel (fixture->device);
  g_assert_true (VALENT_IS_CHANNEL (channel));

  /* Disconnect */
  valent_device_set_channel (fixture->device, NULL);
  g_assert_false (valent_device_get_connected (fixture->device));
}

/*
 * Test pairing
 */
static void
test_device_pairing (DeviceFixture *fixture,
                     gconstpointer  user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  g_autoptr (JsonNode) pair = NULL;
  g_autoptr (JsonNode) unpair = NULL;

  pair = create_pair_packet (TRUE);
  unpair = create_pair_packet (FALSE);

  /* Attach channel */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Send Pair (Request), Receive Unpair (Reject) */
  g_action_group_activate_action (actions, "pair", NULL);
  endpoint_expect_packet_pair (fixture, TRUE);
  endpoint_send_packet (fixture, unpair);
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Send Pair (Request), Receive Pair (Accept) */
  g_action_group_activate_action (actions, "pair", NULL);
  endpoint_expect_packet_pair (fixture, TRUE);
  endpoint_send_packet (fixture, pair);
  g_assert_true (valent_device_get_paired (fixture->device));

  /* Receive Pair (Request), Send Unpair (Reject) */
  valent_device_set_paired (fixture->device, FALSE);
  endpoint_send_packet (fixture, pair);
  g_assert_false (valent_device_get_paired (fixture->device));

  g_action_group_activate_action (actions, "unpair", NULL);
  endpoint_expect_packet_pair (fixture, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Receive Pair (Request), Send Pair (Accept), Send Unpair */
  endpoint_send_packet (fixture, pair);
  g_assert_false (valent_device_get_paired (fixture->device));

  g_action_group_activate_action (actions, "pair", NULL);
  endpoint_expect_packet_pair (fixture, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  g_action_group_activate_action (actions, "unpair", NULL);
  endpoint_expect_packet_pair (fixture, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));


  /* Detach channel */
  valent_device_set_channel (fixture->device, NULL);
  g_assert_false (valent_device_get_connected (fixture->device));
}

static void
test_device_verification_key (DeviceFixture *fixture,
                              gconstpointer  user_data)
{
  g_autoptr (ValentDevice) endpoint_device = NULL;
  JsonNode *endpoint_identity = NULL;
  g_autofree char *channel_verification = NULL;
  g_autofree char *endpoint_verification = NULL;

  endpoint_identity = valent_channel_get_peer_identity (fixture->endpoint);
  endpoint_device = valent_device_new_full (endpoint_identity, NULL);

  /* Check verification key */
  channel_verification = valent_device_get_verification_key (fixture->device);
  endpoint_verification = valent_device_get_verification_key (endpoint_device);
  g_assert_null (channel_verification);
  g_assert_null (endpoint_verification);

  /* Attach channel */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));
  g_assert_false (valent_device_get_paired (fixture->device));

  valent_device_set_channel (endpoint_device, fixture->endpoint);
  g_assert_true (valent_device_get_connected (endpoint_device));
  g_assert_false (valent_device_get_paired (endpoint_device));

  /* Check verification key */
  channel_verification = valent_device_get_verification_key (fixture->device);
  endpoint_verification = valent_device_get_verification_key (endpoint_device);
  g_assert_nonnull (channel_verification);
  g_assert_nonnull (endpoint_verification);
  g_assert_cmpstr (channel_verification, ==, endpoint_verification);

  /* Detach channel */
  valent_device_set_channel (fixture->device, NULL);
  g_assert_false (valent_device_get_connected (fixture->device));
  valent_device_set_channel (endpoint_device, NULL);
  g_assert_false (valent_device_get_connected (endpoint_device));

  // TODO: this shouldn't be necessary
  valent_object_destroy (VALENT_OBJECT (endpoint_device));
}

/*
 * Device Actions
 */
static void
test_device_actions (DeviceFixture *fixture,
                     gconstpointer  user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  g_auto (GStrv) action_names = NULL;
  gboolean has_action = FALSE;
  gboolean enabled = FALSE;
  const GVariantType *parameter_type = NULL;
  const GVariantType *state_type = NULL;
  GVariant *state_hint = NULL;
  GVariant *state = NULL;
  gboolean watch = FALSE;
  PeasPluginInfo *plugin_info;
  g_autoptr (JsonNode) packet = NULL;

  /* Attach channel */
  valent_device_set_channel (fixture->device, fixture->channel);
  valent_device_set_paired (fixture->device, TRUE);

  action_names = g_action_group_list_actions (actions);
  g_assert_cmpuint (g_strv_length (action_names), ==, 5);

  valent_test_watch_signal (actions, "action-added", &watch);
  valent_test_watch_signal (actions, "action-enabled-changed", &watch);
  valent_test_watch_signal (actions, "action-removed", &watch);
  valent_test_watch_signal (actions, "action-state-changed", &watch);

  /* Query */
  has_action = g_action_group_query_action (actions,
                                            "mock.state",
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
  g_action_group_change_action_state (actions,
                                      "mock.state",
                                      g_variant_new_boolean (FALSE));
  valent_test_await_boolean (&watch);

  state = g_action_group_get_action_state (actions, "mock.state");
  g_assert_false (g_variant_get_boolean (state));
  g_clear_pointer (&state, g_variant_unref);

  /* Enable/Disable */
  valent_device_set_paired (fixture->device, FALSE);
  valent_test_await_boolean (&watch);

  valent_device_set_paired (fixture->device, TRUE);
  valent_test_await_boolean (&watch);

  /* Activate */
  g_action_group_activate_action (actions, "mock.echo", NULL);
  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)endpoint_expect_packet_cb,
                              &packet);
  valent_test_await_pointer (&packet);

  v_assert_packet_type (packet, "kdeconnect.mock.echo");

  /* Add/Remove */
  plugin_info = peas_engine_get_plugin_info (valent_get_plugin_engine (),
                                             "packetless");

  peas_engine_unload_plugin (valent_get_plugin_engine (), plugin_info);
  g_assert_false (g_action_group_has_action (actions, "packetless.action"));
  valent_test_await_boolean (&watch);

  peas_engine_load_plugin (valent_get_plugin_engine (), plugin_info);
  g_assert_true (g_action_group_has_action (actions, "packetless.action"));
  valent_test_await_boolean (&watch);

  valent_test_watch_clear (actions, &watch);
}

static inline void
_valent_device_toggle_plugin (ValentDevice   *device,
                              PeasPluginInfo *plugin_info)
{
  g_autofree char *path = NULL;
  g_autoptr (GSettings) settings = NULL;
  gboolean enabled;

  g_assert_true (VALENT_IS_DEVICE (device));
  g_assert_true (PEAS_IS_PLUGIN_INFO (plugin_info));

  path = g_strdup_printf ("/ca/andyholmes/valent/device/%s/plugin/%s/",
                          valent_device_get_id (device),
                          peas_plugin_info_get_module_name (plugin_info));
  settings = g_settings_new_with_path ("ca.andyholmes.Valent.Plugin", path);

  enabled = g_settings_get_boolean (settings, "enabled");
  g_settings_set_boolean (settings, "enabled", !enabled);
}

static void
test_device_plugins (DeviceFixture *fixture,
                     gconstpointer  user_data)
{
  PeasEngine *engine;
  unsigned int n_plugins = 0;

  engine = valent_get_plugin_engine ();
  n_plugins = g_list_model_get_n_items (G_LIST_MODEL (engine));

  VALENT_TEST_CHECK ("Device can unload plugins");
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) plugin_info = NULL;

      plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      if (peas_engine_provides_extension (engine, plugin_info, VALENT_TYPE_DEVICE_PLUGIN))
        peas_engine_unload_plugin (engine, plugin_info);
    }

  VALENT_TEST_CHECK ("Device can load plugins");
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) plugin_info = NULL;

      plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      peas_engine_load_plugin (engine, plugin_info);
    }

  VALENT_TEST_CHECK ("Device can disable plugins");
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) plugin_info = NULL;

      plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      if (peas_engine_provides_extension (engine, plugin_info, VALENT_TYPE_DEVICE_PLUGIN))
        _valent_device_toggle_plugin (fixture->device, plugin_info);
    }

  VALENT_TEST_CHECK ("Device can enable plugins");
  for (unsigned int i = 0; i < n_plugins; i++)
    {
      g_autoptr (PeasPluginInfo) plugin_info = NULL;

      plugin_info = g_list_model_get_item (G_LIST_MODEL (engine), i);
      if (peas_engine_provides_extension (engine, plugin_info, VALENT_TYPE_DEVICE_PLUGIN))
        _valent_device_toggle_plugin (fixture->device, plugin_info);
    }
}

/*
 * Packet Handling
 */
static void
test_handle_packet (DeviceFixture *fixture,
                    gconstpointer  user_data)
{
  JsonNode *packet = get_packet (fixture, "test-echo");

  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  /* Local device is paired, we expect to receive the echo */
  valent_device_set_paired (fixture->device, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  valent_channel_write_packet (fixture->endpoint, packet, NULL, NULL, NULL);
  endpoint_expect_packet_echo (fixture, packet);

  /* Local device is unpaired, we expect to receive a pair packet informing us
   * that the device is unpaired. */
  valent_device_set_paired (fixture->device, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));

  valent_channel_write_packet (fixture->endpoint, packet, NULL, NULL, NULL);
  endpoint_expect_packet_pair (fixture, FALSE);
}

static void
send_available_cb (ValentDevice *device,
                   GAsyncResult *result,
                   gboolean     *done)
{
  g_autoptr (GError) error = NULL;

  valent_device_send_packet_finish (device, result, &error);
  g_assert_no_error (error);

  if (done != NULL)
    *done = TRUE;
}

static void
send_disconnected_cb (ValentDevice *device,
                      GAsyncResult *result,
                      gboolean     *done)
{
  g_autoptr (GError) error = NULL;

  valent_device_send_packet_finish (device, result, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_CONNECTED);

  if (done != NULL)
    *done = TRUE;
}

static void
send_unpaired_cb (ValentDevice *device,
                  GAsyncResult *result,
                  gboolean     *done)
{
  g_autoptr (GError) error = NULL;

  valent_device_send_packet_finish (device, result, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);

  if (done != NULL)
    *done = TRUE;
}

static void
test_send_packet (DeviceFixture *fixture,
                  gconstpointer  user_data)
{
  g_autoptr (JsonNode) pair = create_pair_packet (TRUE);
  gboolean done = FALSE;

  /* Disconnected & Paired */
  g_assert_false (valent_device_get_connected (fixture->device));

  valent_device_set_paired (fixture->device, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  valent_device_send_packet (fixture->device,
                             pair,
                             NULL,
                             (GAsyncReadyCallback)send_disconnected_cb,
                             &done);
  valent_test_await_boolean (&done);

  /* Connected & Paired */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  valent_device_set_paired (fixture->device, TRUE);
  g_assert_true (valent_device_get_paired (fixture->device));

  valent_device_send_packet (fixture->device,
                             pair,
                             NULL,
                             (GAsyncReadyCallback)send_available_cb,
                             &done);
  valent_test_await_boolean (&done);
  endpoint_expect_packet_pair (fixture, TRUE);

  /* Connected & Unpaired */
  valent_device_set_channel (fixture->device, fixture->channel);
  g_assert_true (valent_device_get_connected (fixture->device));

  valent_device_set_paired (fixture->device, FALSE);
  g_assert_false (valent_device_get_paired (fixture->device));

  valent_device_send_packet (fixture->device,
                             pair,
                             NULL,
                             (GAsyncReadyCallback)send_unpaired_cb,
                             &done);
  valent_test_await_boolean (&done);

  /* Cleanup */
  valent_device_set_channel (fixture->device, NULL);
  g_assert_false (valent_device_get_connected (fixture->device));
}

static void
test_device_validate_id (void)
{
  /* See: https://invent.kde.org/network/kdeconnect-android/-/blob/master/tests/org/kde/kdeconnect/DeviceTest.kt
   */
  static struct
  {
    const char *id;
    gboolean    valid;
  } ids[] = {
    // Ideal (hexadecimal; 32 bytes)
    {
      .id = "27456e3cfe5c420896a7c0caeec5e5a0",
      .valid = TRUE,
    },
    {
      .id = "27456E3CfE5C420896A7C0CAEEC5E5A0",
      .valid = TRUE,
    },

    // Compliant (alphanumeric; 32, 38 bytes)
    {
      .id = "abcdefghijklmnopqrstuvwxyz012345",
      .valid = TRUE,
    },
    {
      .id = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789ab",
      .valid = TRUE,
    },

    // Backwards compatible (UUIDv4; 36, 36, 38 bytes)
    {
      .id = "27456e3c-fe5c-4208-96a7-c0caeec5e5a0",
      .valid = TRUE,
    },
    {
      .id = "27456e3c_fe5c_4208_96a7_c0caeec5e5a0",
      .valid = TRUE,
    },
    {
      .id = "_27456e3c_fe5c_4208_96a7_c0caeec5e5a0_",
      .valid = TRUE,
    },

    // Invalid length (numeric; 39, 31, 0 bytes)
    {
      .id = "123456789012345678901234567890123456789",
      .valid = FALSE,
    },
    {
      .id = "1234567890123456789012345678901",
      .valid = FALSE,
    },
    {
      .id = "",
      .valid = FALSE,
    },

    // Invalid characters (32, 34 bytes)
    {
      .id = "!@#$%^&*()!@#$%^&*()!@#$%^&*()!@",
      .valid = FALSE,
    },
    {
      .id = "من بی تو خودم نیستم",
      .valid = FALSE,
    },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (ids); i++)
    {
      if (ids[i].valid)
        g_assert_true (valent_device_validate_id (ids[i].id));
      else
        g_assert_false (valent_device_validate_id (ids[i].id));
    }
}

static void
test_device_validate_name (void)
{
  /* See: https://invent.kde.org/network/kdeconnect-android/-/blob/master/tests/org/kde/kdeconnect/DeviceTest.kt
   */
  static struct
  {
    const char *name;
    gboolean    valid;
  } names[] = {
    {
      .name = "MyDevice",
      .valid = TRUE,
    },
    {
      .name = "    ",
      .valid = FALSE,
    },
    {
      .name = "<><><><><><><><><>",
      .valid = FALSE,
    },
  };

  for (size_t i = 0; i < G_N_ELEMENTS (names); i++)
    {
      if (names[i].valid)
        g_assert_true (valent_device_validate_name (names[i].name));
      else
        g_assert_false (valent_device_validate_name (names[i].name));
    }
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/device/device/new",
                   test_device_new);

  g_test_add ("/libvalent/device/device/basic",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_basic,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device/connecting",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_connecting,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device/pairing",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_pairing,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device/verification-key",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_verification_key,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device/actions",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_actions,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device/plugins",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_device_plugins,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device/handle-packet",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_handle_packet,
              device_fixture_tear_down);

  g_test_add ("/libvalent/device/device/send-packet",
              DeviceFixture, NULL,
              device_fixture_set_up,
              test_send_packet,
              device_fixture_tear_down);

  g_test_add_func ("/libvalent/device/device-validate-id",
                   test_device_validate_id);

  g_test_add_func ("/libvalent/device/device-validate-name",
                   test_device_validate_name);

  return g_test_run ();
}

