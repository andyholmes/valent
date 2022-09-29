// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <libvalent-core.h>
#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "test-sms-common.h"
#include "valent-sms-window.h"

/* This is 50ms longer than the default animation timing for most widgets */
#define TEST_ANIMATION_TIME 250


static GMainLoop *loop = NULL;


static gboolean
test_wait_cb (gpointer data)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static inline void
test_wait (unsigned int timeout_ms)
{
  g_timeout_add (timeout_ms, (GSourceFunc)test_wait_cb, loop);
  g_main_loop_run (loop);
}


static void
test_sms_window (void)
{
  g_autoptr (ValentSmsStore) messages = NULL;
  g_autoptr (ValentContactStore) contacts = NULL;
  g_autoptr (ValentSmsStore) messages_out = NULL;
  g_autoptr (ValentContactStore) contacts_out = NULL;
  ValentSmsWindow *window;

  /* Prepare Stores */
  loop = g_main_loop_new (NULL, FALSE);
  contacts = valent_test_contact_store_new ();
  messages = valent_test_sms_store_new ();

  /* Construction */
  window = g_object_new (VALENT_TYPE_SMS_WINDOW,
                         "contact-store", contacts,
                         "message-store", messages,
                         NULL);
  gtk_window_present (GTK_WINDOW (window));

  /* Let the window load */
  test_wait (500);

  /* Properties */
  contacts_out = valent_sms_window_get_contact_store (window);
  messages_out = valent_sms_window_get_message_store (window);

  g_assert_true (contacts == contacts_out);
  g_assert_true (messages == messages_out);

  g_object_get (window,
                "contact-store", &contacts_out,
                "message-store", &messages_out,
                NULL);

  g_assert_true (contacts == contacts_out);
  g_assert_true (messages == messages_out);

  /* Activate win.new */
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "new",
                                  NULL);
  test_wait (TEST_ANIMATION_TIME);

  valent_sms_window_search_contacts (window, "num");
  test_wait (TEST_ANIMATION_TIME);

  valent_sms_window_search_contacts (window, "123");
  test_wait (TEST_ANIMATION_TIME);

  /* Activate win.previous */
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "previous",
                                  NULL);
  test_wait (TEST_ANIMATION_TIME);

  /* Activate win.search */
  g_action_group_activate_action (G_ACTION_GROUP (window),
                                  "search",
                                  NULL);
  test_wait (TEST_ANIMATION_TIME);

  valent_sms_window_search_messages (window, "Thread");
  test_wait (TEST_ANIMATION_TIME);

  /* Show conversation */
  valent_sms_window_set_active_thread (window, 1);
  test_wait (TEST_ANIMATION_TIME);


  gtk_window_destroy (GTK_WINDOW (window));
  g_clear_pointer (&loop, g_main_loop_unref);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/window",
                   test_sms_window);

  return g_test_run ();
}

