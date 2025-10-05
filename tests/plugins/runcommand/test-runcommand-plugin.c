// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_runcommand_plugin_basic (ValentTestFixture *fixture,
                              gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "runcommand.execute"));

  valent_test_fixture_connect (fixture);

  VALENT_TEST_CHECK ("Plugin action `runcommand.execute` is enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "runcommand.execute"));
}

static void
test_runcommand_plugin_handle_request (ValentTestFixture *fixture,
                                       gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  valent_test_fixture_connect (fixture);

  g_assert_true (g_action_group_get_action_enabled (actions, "runcommand.execute"));

  VALENT_TEST_CHECK ("Plugin sends the command list on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.runcommand");
  v_assert_packet_field (packet, "commandList");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles the command list");
  packet = valent_test_fixture_lookup_packet (fixture, "command-list");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin action `runcommand.execute` sends a request to run a command");
  g_action_group_activate_action (actions,
                                  "runcommand.execute",
                                  g_variant_new_string ("command1"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.runcommand.request");
  v_assert_packet_cmpstr (packet, "key", ==, "command1");
  json_node_unref (packet);
}

static void
test_runcommand_plugin_send_request (ValentTestFixture *fixture,
                                     gconstpointer      user_data)
{
  GVariantDict dict;
  GVariant *command, *commands;
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin sends the command list on connect");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.runcommand");
  v_assert_packet_field (packet, "commandList");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the command list when updated");
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "name", "s", "Test Command");
  g_variant_dict_insert (&dict, "command", "s", "true");
  command = g_variant_dict_end (&dict);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert_value (&dict, "command1", command);
  commands = g_variant_dict_end (&dict);

  g_settings_set_value (fixture->settings, "commands", commands);

  /* Expect the new command list */
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.runcommand");
  v_assert_packet_field (packet, "commandList");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin sends the command list when requested");
  packet = valent_test_fixture_lookup_packet (fixture, "command-list-request");
  valent_test_fixture_handle_packet (fixture, packet);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.runcommand");
  v_assert_packet_field (packet, "commandList");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles a request to run a command");
  packet = valent_test_fixture_lookup_packet (fixture, "command-execute");
  valent_test_fixture_handle_packet (fixture, packet);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-runcommand.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/runcommand/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_runcommand_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/runcommand/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_runcommand_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/runcommand/send-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_runcommand_plugin_send_request,
              valent_test_fixture_clear);

  return g_test_run ();
}
