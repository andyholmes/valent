// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


static void
test_sftp_plugin_basic (ValentTestPluginFixture *fixture,
                         gconstpointer            user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  g_assert_true (g_action_group_has_action (actions, "sftp.browse"));
}

static void
test_sftp_plugin_send_request (ValentTestPluginFixture *fixture,
                               gconstpointer            user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "sftp.browse"));

  /* Request to mount the endpoint */
  g_action_group_activate_action (actions, "sftp.browse", NULL);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sftp.request");
  json_node_unref (packet);

  /* Send an error to the device */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "sftp-error");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Send an request to mount to the device */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "sftp-request");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Expect an error response */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.sftp");
  v_assert_packet_cmpstr (packet, "errorMessage", ==, "Permission denied");
  json_node_unref (packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.sftp.json",
  JSON_SCHEMA_DIR"/kdeconnect.sftp.request.json",
};

static void
test_sftp_plugin_fuzz (ValentTestPluginFixture *fixture,
                        gconstpointer            user_data)

{
  valent_test_plugin_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (unsigned int s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_plugin_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-sftp.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/sftp/basic",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_sftp_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/sftp/send-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_sftp_plugin_send_request,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/sftp/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_sftp_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
