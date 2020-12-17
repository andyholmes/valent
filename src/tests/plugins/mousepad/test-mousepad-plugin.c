#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-input.h>
#include <libvalent-test.h>

#include "kdeconnect.mousepad.echo-fuzz.h"
#include "kdeconnect.mousepad.keyboardstate-fuzz.h"
#include "kdeconnect.mousepad.request-fuzz.h"


static void
mousepad_plugin_fixture_tear_down (ValentTestPluginFixture *fixture,
                                   gconstpointer            user_data)
{
  valent_test_event_free (g_free);
  valent_test_plugin_fixture_clear (fixture, user_data);
}

static void
test_mousepad_plugin_handle_echo (ValentTestPluginFixture *fixture,
                                  gconstpointer            user_data)
{
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Mock Echo */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "echo");
  valent_test_plugin_fixture_handle_packet (fixture, packet);
}

static void
test_mousepad_plugin_handle_request (ValentTestPluginFixture *fixture,
                                     gconstpointer            user_data)
{
  JsonNode *packet;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Pointer Motion */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-motion");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER MOTION 1.0 1.0");

  /* Pointer Scroll */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-axis");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER AXIS 0.0 1.0");

  /* Single Click */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-singleclick");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  /* Double Click */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-doubleclick");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  valent_test_event_cmpstr ("POINTER BUTTON 1 0");
  valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  /* Middle Click */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-middleclick");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER BUTTON 2 1");
  valent_test_event_cmpstr ("POINTER BUTTON 2 0");

  /* Right Click */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-rightclick");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER BUTTON 3 1");
  valent_test_event_cmpstr ("POINTER BUTTON 3 0");

  /* Click */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-singlehold");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER BUTTON 1 1");

  /* Click */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "pointer-singlerelease");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  /* Keypress */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "keyboard-keysym");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("KEYSYM 97 1");
  valent_test_event_cmpstr ("KEYSYM 97 0");

  /* Keypress (Modifiers) */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "keyboard-keysym-mask");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("KEYSYM 65513 1");
  valent_test_event_cmpstr ("KEYSYM 65507 1");
  valent_test_event_cmpstr ("KEYSYM 65505 1");
  valent_test_event_cmpstr ("KEYSYM 65515 1");
  valent_test_event_cmpstr ("KEYSYM 97 1");
  valent_test_event_cmpstr ("KEYSYM 97 0");
  valent_test_event_cmpstr ("KEYSYM 65513 0");
  valent_test_event_cmpstr ("KEYSYM 65507 0");
  valent_test_event_cmpstr ("KEYSYM 65505 0");
  valent_test_event_cmpstr ("KEYSYM 65515 0");

  /* Keypress (Special) */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "keyboard-keysym-special");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  valent_test_event_cmpstr ("KEYSYM 65361 1");
  valent_test_event_cmpstr ("KEYSYM 65361 0");
}

static void
test_mousepad_plugin_send_keyboard_request (ValentTestPluginFixture *fixture,
                                            gconstpointer            user_data)
{
  JsonNode *packet;
  ValentDevice *device;
  GActionGroup *actions;
  GVariantDict dict;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  /* Expect remote state */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.keyboardstate");
  v_assert_packet_true (packet, "state");
  json_node_unref (packet);

  /* Receive endpoint keyboard state */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "keyboardstate-true");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Check event action */
  device = valent_test_plugin_fixture_get_device (fixture);
  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_get_action_enabled (actions, "mousepad-event"));

  /* Send unicode keysym */
  guint keysym, mask;
  gunichar *w;

  /* Send keysym with modifiers */
  keysym = 'a';
  mask = GDK_ALT_MASK | GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_SUPER_MASK;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "keysym", "u", keysym);
  g_variant_dict_insert (&dict, "mask", "u", mask);
  g_action_group_activate_action (actions, "mousepad-event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpstr (packet, "key", ==, "a");
  v_assert_packet_true (packet, "alt");
  v_assert_packet_true (packet, "ctrl");
  v_assert_packet_true (packet, "shift");
  v_assert_packet_true (packet, "super");
  json_node_unref (packet);

  /* Send unicode keysym */
  w = g_utf8_to_ucs4 ("🐱", -1, NULL, NULL, NULL);
  keysym = gdk_unicode_to_keyval (*w);
  mask = 0;
  g_free (w);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "keysym", "u", keysym);
  g_variant_dict_insert (&dict, "mask", "u", mask);
  g_action_group_activate_action (actions, "mousepad-event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpstr (packet, "key", ==, "🐱");
  v_assert_packet_no_field (packet, "alt");
  v_assert_packet_no_field (packet, "ctrl");
  v_assert_packet_no_field (packet, "shift");
  v_assert_packet_no_field (packet, "super");
  json_node_unref (packet);

  /* Send special key (aka non-printable ASCII) */
  // TODO iterate special keys
  keysym = GDK_KEY_F12;
  mask = 0;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "keysym", "u", keysym);
  g_variant_dict_insert (&dict, "mask", "u", mask);
  g_action_group_activate_action (actions, "mousepad-event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpint (packet, "specialKey", ==, 32);
  v_assert_packet_no_field (packet, "alt");
  v_assert_packet_no_field (packet, "ctrl");
  v_assert_packet_no_field (packet, "shift");
  v_assert_packet_no_field (packet, "super");
  json_node_unref (packet);
}

static void
test_mousepad_plugin_send_pointer_request (ValentTestPluginFixture *fixture,
                                           gconstpointer            user_data)
{
  JsonNode *packet;
  ValentDevice *device;
  GActionGroup *actions;
  GVariantDict dict;

  valent_test_plugin_fixture_connect (fixture, TRUE);

  device = valent_test_plugin_fixture_get_device (fixture);
  actions = valent_device_get_actions (device);
  g_assert_true (g_action_group_get_action_enabled (actions, "mousepad-event"));

  /* Expect remote state */
  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.keyboardstate");
  v_assert_packet_true (packet, "state");
  json_node_unref (packet);

  /* Receive endpoint keyboard state */
  packet = valent_test_plugin_fixture_lookup_packet (fixture, "keyboardstate-true");
  valent_test_plugin_fixture_handle_packet (fixture, packet);

  /* Pointer Motion */
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "dx", "d", 1.0);
  g_variant_dict_insert (&dict, "dy", "d", 1.0);
  g_action_group_activate_action (actions, "mousepad-event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpfloat (packet, "dx", ==, 1.0);
  v_assert_packet_cmpfloat (packet, "dy", ==, 1.0);
  json_node_unref (packet);

  /* Pointer Axis */
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "dx", "d", 0.0);
  g_variant_dict_insert (&dict, "dy", "d", 1.0);
  g_variant_dict_insert (&dict, "scroll", "b", TRUE);
  g_action_group_activate_action (actions, "mousepad-event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_plugin_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpfloat (packet, "dx", ==, 0.0);
  v_assert_packet_cmpfloat (packet, "dy", ==, 1.0);
  v_assert_packet_true (packet, "scroll");
  json_node_unref (packet);
}

static void
test_mousepad_plugin_fuzz (ValentTestPluginFixture *fixture,
                           gconstpointer            user_data)

{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *packet = NULL;

  valent_test_plugin_fixture_connect (fixture, TRUE);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  parser = json_parser_new ();

  for (unsigned int i = 0; i < G_N_ELEMENTS (mousepad_echo_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  mousepad_echo_fuzz[i].json,
                                  mousepad_echo_fuzz[i].size,
                                  NULL);
      packet = json_parser_get_root (parser);

      if (VALENT_IS_PACKET (packet))
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }

  for (unsigned int i = 0; i < G_N_ELEMENTS (mousepad_keyboardstate_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  mousepad_keyboardstate_fuzz[i].json,
                                  mousepad_keyboardstate_fuzz[i].size,
                                  NULL);
      packet = json_parser_get_root (parser);

      if (VALENT_IS_PACKET (packet))
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }

  for (unsigned int i = 0; i < G_N_ELEMENTS (mousepad_request_fuzz); i++)
    {
      json_parser_load_from_data (parser,
                                  mousepad_request_fuzz[i].json,
                                  mousepad_request_fuzz[i].size,
                                  NULL);
      packet = json_parser_get_root (parser);

      if (VALENT_IS_PACKET (packet))
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }
}

int
main (int   argc,
      char *argv[])
{
  const char *path = TEST_DATA_DIR"/plugin-mousepad.json";

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/mousepad/handle-echo",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_mousepad_plugin_handle_echo,
              mousepad_plugin_fixture_tear_down);

  g_test_add ("/plugins/mousepad/handle-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_mousepad_plugin_handle_request,
              mousepad_plugin_fixture_tear_down);

  g_test_add ("/plugins/mousepad/send-keyboard-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_mousepad_plugin_send_keyboard_request,
              mousepad_plugin_fixture_tear_down);

  g_test_add ("/plugins/mousepad/send-pointer-request",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_mousepad_plugin_send_pointer_request,
              mousepad_plugin_fixture_tear_down);

#ifdef VALENT_TEST_FUZZ
  g_test_add ("/plugins/mousepad/fuzz",
              ValentTestPluginFixture, path,
              valent_test_plugin_fixture_init,
              test_mousepad_plugin_fuzz,
              mousepad_plugin_fixture_tear_down);
#endif

  return g_test_run ();
}
