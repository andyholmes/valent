// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mousepad-keydef.h"

static ValentInputAdapter *default_adapter = NULL;
static GQueue events = G_QUEUE_INIT;

#define _valent_test_event_cmpstr(str)             \
  G_STMT_START {                                   \
    char *_event_str = g_queue_pop_head (&events); \
    g_assert_cmpstr (_event_str, ==, str);         \
    g_free (_event_str);                           \
  } G_STMT_END

static void
on_event_state_changed (GActionGroup *group,
                        const char   *name,
                        GVariant     *value,
                        gpointer      user_data)
{
  g_queue_push_tail (&events, g_variant_dup_string (value, NULL));
}

static void
mousepad_plugin_fixture_set_up (ValentTestFixture *fixture,
                                gconstpointer      user_data)
{
  valent_test_fixture_init (fixture, user_data);

  default_adapter = valent_test_await_adapter (valent_input_get_default ());
  g_signal_connect_object (default_adapter,
                           "action-state-changed::event",
                           G_CALLBACK (on_event_state_changed),
                           NULL,
                           G_CONNECT_DEFAULT);
}

static void
mousepad_plugin_fixture_tear_down (ValentTestFixture *fixture,
                                   gconstpointer      user_data)
{
  g_signal_handlers_disconnect_by_func (default_adapter, on_event_state_changed, NULL);
  g_queue_clear_full (&events, g_free);

  valent_test_fixture_clear (fixture, user_data);
}

static void
test_mousepad_plugin_handle_echo (ValentTestFixture *fixture,
                                  gconstpointer      user_data)
{
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin sends the keyboard state on connect");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.keyboardstate");
  v_assert_packet_true (packet, "state");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin handles an even echo when received");
  packet = valent_test_fixture_lookup_packet (fixture, "echo");
  valent_test_fixture_handle_packet (fixture, packet);
}

static void
test_mousepad_plugin_handle_request (ValentTestFixture *fixture,
                                     gconstpointer      user_data)
{
  JsonNode *packet;

  VALENT_TEST_CHECK ("Plugin sends the keyboard state on connect");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.keyboardstate");
  v_assert_packet_true (packet, "state");
  json_node_unref (packet);

  /* Pointer Motion */
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-motion");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER MOTION 1.0 1.0");

  VALENT_TEST_CHECK ("Plugin handles a request to scroll the pointer");
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-axis");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER AXIS 0.0 1.0");

  VALENT_TEST_CHECK ("Plugin handles a request to perform a single click");
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-singleclick");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  _valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  VALENT_TEST_CHECK ("Plugin handles a request to perform a double click");
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-doubleclick");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  _valent_test_event_cmpstr ("POINTER BUTTON 1 0");
  _valent_test_event_cmpstr ("POINTER BUTTON 1 1");
  _valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  VALENT_TEST_CHECK ("Plugin handles a request to perform a middle click");
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-middleclick");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER BUTTON 2 1");
  _valent_test_event_cmpstr ("POINTER BUTTON 2 0");

  VALENT_TEST_CHECK ("Plugin handles a request to perform a right click");
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-rightclick");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER BUTTON 3 1");
  _valent_test_event_cmpstr ("POINTER BUTTON 3 0");

  VALENT_TEST_CHECK ("Plugin handles a request to perform a single hold");
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-singlehold");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER BUTTON 1 1");

  VALENT_TEST_CHECK ("Plugin handles a request to perform a single release");
  packet = valent_test_fixture_lookup_packet (fixture, "pointer-singlerelease");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("POINTER BUTTON 1 0");

  VALENT_TEST_CHECK ("Plugin handles a request to press-release a keysym");
  packet = valent_test_fixture_lookup_packet (fixture, "keyboard-keysym");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("KEYSYM 97 1");
  _valent_test_event_cmpstr ("KEYSYM 97 0");

  VALENT_TEST_CHECK ("Plugin handles a request to press-release a keysym with modifiers");
  packet = valent_test_fixture_lookup_packet (fixture, "keyboard-keysym-mask");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("KEYSYM 65513 1");
  _valent_test_event_cmpstr ("KEYSYM 65507 1");
  _valent_test_event_cmpstr ("KEYSYM 65505 1");
  _valent_test_event_cmpstr ("KEYSYM 65515 1");
  _valent_test_event_cmpstr ("KEYSYM 97 1");
  _valent_test_event_cmpstr ("KEYSYM 97 0");
  _valent_test_event_cmpstr ("KEYSYM 65513 0");
  _valent_test_event_cmpstr ("KEYSYM 65507 0");
  _valent_test_event_cmpstr ("KEYSYM 65505 0");
  _valent_test_event_cmpstr ("KEYSYM 65515 0");

  VALENT_TEST_CHECK ("Plugin handles a request to press-release a special key");
  packet = valent_test_fixture_lookup_packet (fixture, "keyboard-keysym-special");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("KEYSYM 65361 1");
  _valent_test_event_cmpstr ("KEYSYM 65361 0");

  VALENT_TEST_CHECK ("Plugin handles a request to press-release a series of keysyms");
  packet = valent_test_fixture_lookup_packet (fixture, "keyboard-keysym-string");
  valent_test_fixture_handle_packet (fixture, packet);

  _valent_test_event_cmpstr ("KEYSYM 116 1");
  _valent_test_event_cmpstr ("KEYSYM 116 0");
  _valent_test_event_cmpstr ("KEYSYM 101 1");
  _valent_test_event_cmpstr ("KEYSYM 101 0");
  _valent_test_event_cmpstr ("KEYSYM 115 1");
  _valent_test_event_cmpstr ("KEYSYM 115 0");
  _valent_test_event_cmpstr ("KEYSYM 116 1");
  _valent_test_event_cmpstr ("KEYSYM 116 0");
}

static void
test_mousepad_plugin_send_keyboard_request (ValentTestFixture *fixture,
                                            gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;
  GVariantDict dict;
  gboolean watch = FALSE;

  valent_test_watch_signal (actions,
                            "action-enabled-changed::mousepad.event",
                            &watch);

  VALENT_TEST_CHECK ("Plugin sends the keyboard state on connect");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.keyboardstate");
  v_assert_packet_true (packet, "state");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` is disabled when `keyboardstate` is `false`");
  g_assert_false (g_action_group_get_action_enabled (actions, "mousepad.event"));

  VALENT_TEST_CHECK ("Plugin handles the keyboard state");
  packet = valent_test_fixture_lookup_packet (fixture, "keyboardstate-true");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` is enabled");
  g_assert_true (g_action_group_get_action_enabled (actions, "mousepad.event"));

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` sends ASCII with modifiers");
  uint32_t keysym;
  unsigned int mask;
  gunichar *w;

  keysym = 'a';
  mask = KEYMOD_KDE_MASK;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "keysym", "u", keysym);
  g_variant_dict_insert (&dict, "mask", "u", mask);
  g_action_group_activate_action (actions, "mousepad.event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpstr (packet, "key", ==, "a");
  v_assert_packet_true (packet, "alt");
  v_assert_packet_true (packet, "ctrl");
  v_assert_packet_true (packet, "shift");
  v_assert_packet_true (packet, "super");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` sends unicode keysyms");
  w = g_utf8_to_ucs4 ("🐱", -1, NULL, NULL, NULL);
  keysym = valent_input_unicode_to_keysym (*w);
  mask = 0;
  g_free (w);

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "keysym", "u", keysym);
  g_variant_dict_insert (&dict, "mask", "u", mask);
  g_action_group_activate_action (actions, "mousepad.event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpstr (packet, "key", ==, "🐱");
  v_assert_packet_no_field (packet, "alt");
  v_assert_packet_no_field (packet, "ctrl");
  v_assert_packet_no_field (packet, "shift");
  v_assert_packet_no_field (packet, "super");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` sends special keys "
                     "(aka non-printable ASCII");
  // TODO iterate special keys
  keysym = KEYSYM_F12;
  mask = 0;

  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "keysym", "u", keysym);
  g_variant_dict_insert (&dict, "mask", "u", mask);
  g_action_group_activate_action (actions, "mousepad.event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpint (packet, "specialKey", ==, 32);
  v_assert_packet_no_field (packet, "alt");
  v_assert_packet_no_field (packet, "ctrl");
  v_assert_packet_no_field (packet, "shift");
  v_assert_packet_no_field (packet, "super");
  json_node_unref (packet);

  valent_test_watch_clear (actions, &watch);
}

static void
test_mousepad_plugin_send_pointer_request (ValentTestFixture *fixture,
                                           gconstpointer      user_data)
{
  GActionGroup *actions = G_ACTION_GROUP (fixture->device);
  JsonNode *packet;
  GVariantDict dict;
  gboolean watch = FALSE;

  valent_test_watch_signal (actions,
                            "action-enabled-changed::mousepad.event",
                            &watch);

  VALENT_TEST_CHECK ("Plugin sends the keyboard state on connect");
  valent_test_fixture_connect (fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.keyboardstate");
  v_assert_packet_true (packet, "state");
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` is disabled when `keyboardstate` is `false`");
  g_assert_false (g_action_group_get_action_enabled (actions, "mousepad.event"));

  VALENT_TEST_CHECK ("Plugin handles the keyboard state");
  packet = valent_test_fixture_lookup_packet (fixture, "keyboardstate-true");
  valent_test_fixture_handle_packet (fixture, packet);
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` is enabled when `keyboardstate` is `true`");
  g_assert_true (g_action_group_get_action_enabled (actions, "mousepad.event"));

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` moves the pointer");
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "dx", "d", 1.0);
  g_variant_dict_insert (&dict, "dy", "d", 1.0);
  g_action_group_activate_action (actions, "mousepad.event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpfloat (packet, "dx", >=, 1.0);
  v_assert_packet_cmpfloat (packet, "dy", >=, 1.0);
  json_node_unref (packet);

  VALENT_TEST_CHECK ("Plugin action `mousepad.event` moves the pointer axis");
  g_variant_dict_init (&dict, NULL);
  g_variant_dict_insert (&dict, "dx", "d", 0.0);
  g_variant_dict_insert (&dict, "dy", "d", 1.0);
  g_variant_dict_insert (&dict, "scroll", "b", TRUE);
  g_action_group_activate_action (actions, "mousepad.event",
                                  g_variant_dict_end (&dict));

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.request");
  v_assert_packet_cmpfloat (packet, "dx", <=, 0.0);
  v_assert_packet_cmpfloat (packet, "dy", >=, 1.0);
  v_assert_packet_true (packet, "scroll");
  json_node_unref (packet);

  valent_test_watch_clear (actions, &watch);
}

static const char *schemas[] = {
  "/tests/kdeconnect.mousepad.echo.json",
  "/tests/kdeconnect.mousepad.keyboardstate.json",
  "/tests/kdeconnect.mousepad.request.json",
};

static void
test_mousepad_plugin_fuzz (ValentTestFixture *fixture,
                           gconstpointer      user_data)

{
  JsonNode *packet;

  valent_test_fixture_connect (fixture);
  g_test_log_set_fatal_handler (valent_test_mute_fuzzing, NULL);

  VALENT_TEST_CHECK ("Plugin sends the keyboard state on connect");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.mousepad.keyboardstate");
  v_assert_packet_true (packet, "state");
  json_node_unref (packet);

  for (size_t s = 0; s < G_N_ELEMENTS (schemas); s++)
    valent_test_fixture_schema_fuzz (fixture, schemas[s]);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mousepad.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mousepad/handle-echo",
              ValentTestFixture, path,
              mousepad_plugin_fixture_set_up,
              test_mousepad_plugin_handle_echo,
              mousepad_plugin_fixture_tear_down);

  g_test_add ("/plugins/mousepad/handle-request",
              ValentTestFixture, path,
              mousepad_plugin_fixture_set_up,
              test_mousepad_plugin_handle_request,
              mousepad_plugin_fixture_tear_down);

  g_test_add ("/plugins/mousepad/send-keyboard-request",
              ValentTestFixture, path,
              mousepad_plugin_fixture_set_up,
              test_mousepad_plugin_send_keyboard_request,
              mousepad_plugin_fixture_tear_down);

  g_test_add ("/plugins/mousepad/send-pointer-request",
              ValentTestFixture, path,
              mousepad_plugin_fixture_set_up,
              test_mousepad_plugin_send_pointer_request,
              mousepad_plugin_fixture_tear_down);

  g_test_add ("/plugins/mousepad/fuzz",
              ValentTestFixture, path,
              mousepad_plugin_fixture_set_up,
              test_mousepad_plugin_fuzz,
              mousepad_plugin_fixture_tear_down);

  return g_test_run ();
}
