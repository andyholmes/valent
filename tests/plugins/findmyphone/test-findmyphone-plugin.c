// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_findmyphone_plugin_basic (ValentTestFixture *fixture,
                               gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  g_assert_true (g_action_group_has_action (actions, "findmyphone.ring"));
}

static void
test_findmyphone_plugin_handle_request (ValentTestFixture *fixture,
                                        gconstpointer      user_data)
{
  JsonNode *packet;

  packet = valent_test_fixture_lookup_packet (fixture, "ring-request");

  /* Start ringing */
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_timeout (1);

  /* Stop ringing */
  valent_test_fixture_handle_packet (fixture, packet);
}

static void
test_findmyphone_plugin_send_request (ValentTestFixture *fixture,
                                      gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;

  valent_test_fixture_connect (fixture, TRUE);

  g_assert_true (g_action_group_get_action_enabled (actions, "findmyphone.ring"));

  g_action_group_activate_action (actions, "findmyphone.ring", NULL);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.findmyphone.request");
  json_node_unref (packet);
}

static const char *schemas[] = {
  "/tests/kdeconnect.findmyphone.request.json",
};

static void
test_findmyphone_plugin_fuzz (ValentTestFixture *fixture,
                              gconstpointer      user_data)

{
  valent_test_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  for (unsigned int s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-findmyphone.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/findmyphone/basic",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_findmyphone_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/findmyphone/handle-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_findmyphone_plugin_handle_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/findmyphone/send-request",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_findmyphone_plugin_send_request,
              valent_test_fixture_clear);

  g_test_add ("/plugins/findmyphone/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_findmyphone_plugin_fuzz,
              valent_test_fixture_clear);

  if (gst_is_initialized ())
    gst_deinit ();

  return g_test_run ();
}
