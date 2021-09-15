// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


static ValentNotificationSource *source = NULL;

static void
notification_plugin_fixture_set_up (ValentTestPluginFixture *fixture,
                                    gconstpointer            user_data)
{
  valent_test_plugin_fixture_init (fixture, user_data);
  valent_test_plugin_fixture_init_settings (fixture, "notification");

  // TODO: test with session active/inactive
  source = valent_mock_notification_source_get_instance ();
}

static void
test_notification_plugin_basic (ValentTestPluginFixture *fixture,
                                gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  g_assert_true (g_action_group_has_action (actions, "notification-action"));
  g_assert_true (g_action_group_has_action (actions, "notification-cancel"));
  g_assert_true (g_action_group_has_action (actions, "notification-close"));
  g_assert_true (g_action_group_has_action (actions, "notification-reply"));
  g_assert_true (g_action_group_has_action (actions, "notification-send"));
}

static void
test_notification_plugin_handle_notification (ValentTestPluginFixture *fixture,
                                              gconstpointer            user_data)
{
  JsonNode *packet;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = NULL;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  /* Receive a simple notification */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "notification-simple");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Receive a notification with an icon */
  file = g_file_new_for_uri ("file://"TEST_DATA_DIR"image.png");
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "notification-icon");
  valent_test_plugin_fixture_upload (fixture, packet, file, &error);
  g_assert_no_error (error);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Receive a notification with actions */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "notification-actions");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Receive a repliable notification */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "notification-repliable");
  valent_test_plugin_fixture_handle_packet (fixture, packet);
}

static void
test_notification_plugin_send_notification (ValentTestPluginFixture *fixture,
                                            gconstpointer            user_data)
{
  JsonNode *packet;
  g_autoptr (ValentNotification) notification = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GIcon) icon = NULL;

  /* Send when active */
  g_settings_set_boolean (fixture->settings, "forward-when-active", TRUE);

  /* Expect notification request */
  valent_test_plugin_fixture_connect (fixture, TRUE);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  /* Send an empty notification */
  notification = valent_notification_new (NULL);
  valent_notification_source_emit_notification_added (source, notification);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_field (packet, "id");
  v_assert_packet_field (packet, "appName");
  v_assert_packet_field (packet, "title");
  v_assert_packet_field (packet, "body");
  v_assert_packet_field (packet, "ticker");
  json_node_unref (packet);

  /* Send a standard notification */
  valent_notification_set_id (notification, "test-id");
  valent_notification_set_application (notification, "Test Application");
  valent_notification_set_title (notification, "Test Title");
  valent_notification_set_body (notification, "Test Body");
  valent_notification_source_emit_notification_added (source, notification);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");
  json_node_unref (packet);

  /* Send a notification with a themed icon */
  valent_notification_set_icon_from_string (notification,
                                            "dialog-information-symbolic");
  valent_notification_source_emit_notification_added (source, notification);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");

  if (valent_packet_has_payload (packet))
    valent_test_plugin_fixture_download (fixture, packet, NULL);

  json_node_unref (packet);

  /* Send a notification with a file icon */
  valent_notification_set_icon_from_string (notification,
                                            "file://"TEST_DATA_DIR"image.png");
  valent_notification_source_emit_notification_added (source, notification);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_plugin_fixture_download (fixture, packet, NULL);
  json_node_unref (packet);

  /* Send a notification with a bytes icon */
  file = g_file_new_for_uri ("file://"TEST_DATA_DIR"image.png");
  bytes = g_file_load_bytes (file, NULL, NULL, NULL);
  icon = g_bytes_icon_new (bytes);
  valent_notification_set_icon (notification, icon);
  valent_notification_source_emit_notification_added (source, notification);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_plugin_fixture_download (fixture, packet, NULL);
  json_node_unref (packet);
}

static void
test_notification_plugin_actions (ValentTestPluginFixture *fixture,
                                  gconstpointer            user_data)
{
  ValentDevice *device;
  GActionGroup *actions;
  JsonNode *packet;
  GVariantDict dict;
  GIcon *icon;
  GVariant *iconv;
  GVariant *variant;

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);

  /* Expect notification request */
  valent_test_plugin_fixture_connect (fixture, TRUE);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_true (packet, "request");
  json_node_unref (packet);

  /* Send a notification with a themed icon */
  icon = g_themed_icon_new ("dialog-information-symbolic");
  iconv = g_icon_serialize (icon);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "id", "s", "test-id");
  g_variant_dict_insert (&dict, "application", "s", "Test Application");
  g_variant_dict_insert (&dict, "title", "s", "Test Title");
  g_variant_dict_insert (&dict, "body", "s", "Test Body");
  g_variant_dict_insert_value (&dict, "icon", iconv);
  g_action_group_activate_action (actions, "notification-send",
                                  g_variant_dict_end (&dict));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_cmpstr (packet, "appName", ==, "Test Application");
  v_assert_packet_cmpstr (packet, "title", ==, "Test Title");
  v_assert_packet_cmpstr (packet, "body", ==, "Test Body");
  v_assert_packet_cmpstr (packet, "ticker", ==, "Test Title: Test Body");

  if (valent_packet_has_payload (packet))
    valent_test_plugin_fixture_download (fixture, packet, NULL);

  g_object_unref (icon);
  g_variant_unref (iconv);
  json_node_unref (packet);

  /* Send an activation for a notification action */
  variant = g_variant_new ("(ss)", "test-id", "Test Action");
  g_action_group_activate_action (actions, "notification-action", variant);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.action");
  v_assert_packet_cmpstr (packet, "key", ==, "test-id");
  v_assert_packet_cmpstr (packet, "action", ==, "Test Action");
  json_node_unref (packet);

  /* Send cancellation of a local notification */
  variant = g_variant_new_string ("test-id");
  g_action_group_activate_action (actions, "notification-cancel", variant);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification");
  v_assert_packet_cmpstr (packet, "id", ==, "test-id");
  v_assert_packet_true (packet, "isCancel");
  json_node_unref (packet);

  /* Request closing a remote notification */
  variant = g_variant_new_string ("test-id");
  g_action_group_activate_action (actions, "notification-close", variant);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.request");
  v_assert_packet_cmpstr (packet, "cancel", ==, "test-id");
  json_node_unref (packet);

  /* Send a reply for a repliable notification */
  variant = g_variant_new ("(ssv)", "test-id", "Test Reply",
                           g_variant_new_string (""));
  g_action_group_activate_action (actions, "notification-reply", variant);

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.notification.reply");
  v_assert_packet_cmpstr (packet, "requestReplyId", ==, "test-id");
  v_assert_packet_cmpstr (packet, "message", ==, "Test Reply");
  json_node_unref (packet);
}

static const char *schemas[] = {
  JSON_SCHEMA_DIR"/kdeconnect.notification.json",
  JSON_SCHEMA_DIR"/kdeconnect.notification.action.json",
  JSON_SCHEMA_DIR"/kdeconnect.notification.reply.json",
  JSON_SCHEMA_DIR"/kdeconnect.notification.request.json",
};

static void
test_notification_plugin_fuzz (ValentTestPluginFixture *fixture,
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
  const char *path = TEST_DATA_DIR"/plugin-notification.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  if (!gtk_init_check ())
    g_test_message ("Skipping themed icon transfers");

  g_test_add ("/plugins/notification/basic",
              ValentTestPluginFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_basic,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/notification/handle-notification",
              ValentTestPluginFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_handle_notification,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/notification/send-notification",
              ValentTestPluginFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_send_notification,
              valent_test_plugin_fixture_clear);

  g_test_add ("/plugins/notification/actions",
              ValentTestPluginFixture, path,
              notification_plugin_fixture_set_up,
              test_notification_plugin_actions,
              valent_test_plugin_fixture_clear);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/notification/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_notification_plugin_fuzz,
              valent_test_plugin_fixture_clear);
#endif

  return g_test_run ();
}
