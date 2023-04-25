// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>


static ValentNotificationsAdapter *adapter = NULL;

static void
notification_plugin_fixture_set_up (ValentTestFixture *fixture,
                                    gconstpointer      user_data)
{
  valent_test_fixture_init (fixture, user_data);

  adapter = valent_test_await_adapter (valent_notifications_get_default ());
}

static void
test_notification_plugin_basic (ValentTestFixture *fixture,
                                gconstpointer            user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);

  VALENT_TEST_CHECK ("Plugin has expected actions");
  g_assert_true (g_action_group_has_action (actions, "notification.action"));
  g_assert_true (g_action_group_has_action (actions, "notification.cancel"));
  g_assert_true (g_action_group_has_action (actions, "notification.close"));
  g_assert_true (g_action_group_has_action (actions, "notification.reply"));
  g_assert_true (g_action_group_has_action (actions, "notification.send"));

  valent_test_fixture_connect (fixture, TRUE);

  VALENT_TEST_CHECK ("Plugin action `notification.action` is enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "notification.action"));

  VALENT_TEST_CHECK ("Plugin action `notification.cancel` is enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "notification.cancel"));

  VALENT_TEST_CHECK ("Plugin action `notification.close` is enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "notification.close"));

  VALENT_TEST_CHECK ("Plugin action `notification.reply` is enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "notification.reply"));

  VALENT_TEST_CHECK ("Plugin action `notification.send` is enabled when connected");
  g_assert_true (g_action_group_get_action_enabled (actions, "notification.send"));
}

static void
test_notification_plugin_handle_notification (ValentTestFixture *fixture,
                                              gconstpointer      user_data)
{
  JsonNode *packet;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;

  VALENT_TEST_CHECK ("Plugin requests the existing notifications on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles a simple notification");
  packet = valent_test_fixture_lookup_packet (fixture, "notification-simple");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles a notification with an icon");
  file = g_file_new_for_uri ("resource:///tests/image.png");
  packet = valent_test_fixture_lookup_packet (fixture, "notification-icon");
  valent_test_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  // FIXME: Without this the notification plugin will reliably segfault, which
  //        ostensibly implies ValentDevicePlugin is not thread-safe
  valent_test_await_timeout (1000);

  VALENT_TEST_CHECK ("Plugin handles a notification with actions");
  packet = valent_test_fixture_lookup_packet (fixture, "notification-actions");
  valent_test_fixture_handle_packet (fixture, packet);

  VALENT_TEST_CHECK ("Plugin handles a repliable notification");
  packet = valent_test_fixture_lookup_packet (fixture, "notification-repliable");
  valent_test_fixture_handle_packet (fixture, packet);
}

static void
test_notification_plugin_send_notification (ValentTestFixture *fixture,
                                            gconstpointer      user_data)
{
  JsonNode *packet;
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GIcon) icon = NULL;
  GError *error = NULL;

  /* TODO: Send when active */
  g_settings_set_boolean (fixture->settings, "forward-when-active", TRUE);

  VALENT_TEST_CHECK ("Plugin requests the existing notifications on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards simple notifications");
  notification = valent_notification_new (NULL);
  valent_notifications_adapter_notification_added (adapter, notification);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_field (packet, "id");
  v_assert_packet_field (packet, "appName");
  v_assert_packet_field (packet, "title");
  v_assert_packet_field (packet, "body");
  v_assert_packet_field (packet, "ticker");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards standard notifications");
  valent_notification_set_id (notification, "test-id");
  valent_notification_set_application (notification, "Test Application");
  valent_notification_set_title (notification, "Test Title");
  valent_notification_set_body (notification, "Test Body");
  valent_notifications_adapter_notification_added (adapter, notification);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards notifications with themed icons");
  icon = g_themed_icon_new ("dialog-information-symbolic");
  valent_notification_set_icon (notification, icon);
  valent_notifications_adapter_notification_added (adapter, notification);
  g_clear_object (&icon);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");

  if (valent_packet_has_payload (packet))
    {
      valent_test_fixture_download (fixture, packet, &error);
      g_assert_no_error (error);
    }
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards notifications with file icons");
  file = g_file_new_for_uri ("resource:///tests/image.png");
  icon = g_file_icon_new (file);
  valent_notification_set_icon (notification, icon);
  valent_notifications_adapter_notification_added (adapter, notification);
  g_clear_object (&file);
  g_clear_object (&icon);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_fixture_download (fixture, packet, NULL);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards notifications with bytes icons");
  file = g_file_new_for_uri ("resource:///tests/image.png");
  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  icon = g_bytes_icon_new (bytes);
  valent_notification_set_icon (notification, icon);
  valent_notifications_adapter_notification_added (adapter, notification);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_fixture_download (fixture, packet, NULL);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin forwards notification removals");
  valent_notifications_adapter_notification_removed (adapter, "test-id");

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_cmpstr (packet, "cancel", ==, "test-id");
  json_node_unref (packet);
}

static void
test_notification_plugin_actions (ValentTestFixture *fixture,
                                  gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;
  g_autoptr (GIcon) icon = NULL;
  g_autoptr (ValentNotification) notification = NULL;
  GError *error = NULL;

  VALENT_TEST_CHECK ("Plugin requests the existing notifications on connect");
  valent_test_fixture_connect (fixture, TRUE);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `notification.send` forwards notifications");
  icon = g_themed_icon_new ("dialog-information-symbolic");
  notification = g_object_new (VALENT_TYPE_NOTIFICATION,
                               "id",          "test-id",
                               "application", "Test Application",
                               "title",       "Test Title",
                               "body",        "Test Body",
                               "icon",        icon,
                               NULL);

  g_action_group_activate_action (actions, "notification.send",
                                  valent_notification_serialize (notification));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");

  if (valent_packet_has_payload (packet))
    {
      valent_test_fixture_download (fixture, packet, &error);
      g_assert_no_error (error);
    }
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `notification.action` forwards activations");
  g_action_group_activate_action (actions,
                                  "notification.action",
                                  g_variant_new ("(ss)",
                                                 "test-id",
                                                 "Test Action"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.action");
  v_assert_packet_cmpstr (packet, "key", ==, "test-id");
  v_assert_packet_cmpstr (packet, "action", ==, "Test Action");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `notification.cancel` forwards notification removals");
  g_action_group_activate_action (actions,
                                  "notification.cancel",
                                  g_variant_new_string ("test-id"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_true (packet, "isCancel");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `notification.cancel` sends a request to close a notification");
  g_action_group_activate_action (actions,
                                  "notification.close",
                                  g_variant_new_string ("test-id"));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_cmpstr (packet, "cancel", ==, "test-id");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `notification.reply` sends a reply to a notification");
  g_action_group_activate_action (actions,
                                  "notification.reply",
                                  g_variant_new ("(ssv)",
                                                 "test-id",
                                                 "Test Reply",
                                                 g_variant_new_string (""))); // FIXME

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.reply");
  v_assert_packet_cmpstr (packet, "requestReplyId", ==, "test-id");
  v_assert_packet_cmpstr (packet, "message", ==, "Test Reply");
  json_node_unref (packet);
}

static const char *schemas[] = {
  "/tests/kdeconnect.notification.json",
  "/tests/kdeconnect.notification.action.json",
  "/tests/kdeconnect.notification.reply.json",
  "/tests/kdeconnect.notification.request.json",
};

static void
test_notification_plugin_fuzz (ValentTestFixture *fixture,
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
  const char *path = "plugin-notification.json";

  valent_test_init (&argc, &argv, NULL);

  if (!gtk_init_check ())
    g_test_message ("Skipping themed icon transfers");

  g_test_add ("/plugins/notification/basic",
              ValentTestFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_basic,
              valent_test_fixture_clear);

  g_test_add ("/plugins/notification/handle-notification",
              ValentTestFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_handle_notification,
              valent_test_fixture_clear);

  g_test_add ("/plugins/notification/send-notification",
              ValentTestFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_send_notification,
              valent_test_fixture_clear);

  g_test_add ("/plugins/notification/actions",
              ValentTestFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_actions,
              valent_test_fixture_clear);

  g_test_add ("/plugins/notification/fuzz",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_notification_plugin_fuzz,
              valent_test_fixture_clear);

  return g_test_run ();
}
