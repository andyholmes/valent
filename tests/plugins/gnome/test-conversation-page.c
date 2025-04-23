// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#include "valent-conversation-page.h"


static void
test_conversation_page (void)
{
  GtkWidget *conversation;
  GtkWidget *window;
  g_autoptr (ValentContactsAdapter) contacts = NULL;
  g_autoptr (ValentMessagesAdapter) messages = NULL;
  g_autoptr (ValentContactsAdapter) contacts_out = NULL;
  g_autoptr (ValentMessagesAdapter) messages_out = NULL;
  g_autofree char *iri = NULL;
  g_autofree char *iri_out = NULL;

  VALENT_TEST_CHECK ("Widget can be constructed");
  conversation = g_object_new (VALENT_TYPE_CONVERSATION_PAGE,
                               "contacts", contacts,
                               "messages", messages,
                               "iri",      iri,
                               NULL);

  VALENT_TEST_CHECK ("Widget can be realized");
  window = g_object_new (GTK_TYPE_WINDOW,
                         "child",          conversation,
                         "default-height", 480,
                         "default-width",  600,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (GTK_WINDOW (window));
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (conversation,
                "contacts", &contacts_out,
                "messages", &messages_out,
                "iri",      &iri_out,
                NULL);
  g_assert_true (contacts == contacts_out);
  g_assert_true (messages == messages_out);
  g_assert_cmpstr (iri_out, ==, valent_conversation_page_get_iri (VALENT_CONVERSATION_PAGE (conversation)));

  gtk_window_destroy (GTK_WINDOW (window));
  valent_test_await_nullptr (&window);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/conversation-page",
                   test_conversation_page);

  return g_test_run ();
}

