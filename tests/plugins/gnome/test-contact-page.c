// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#include "valent-contact-page.h"


static void
test_contact_page (void)
{
  GtkWidget *page;
  GtkWidget *window;
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (ValentContactsAdapter) contacts = NULL;
  g_autoptr (ValentContactsAdapter) contacts_out = NULL;
  g_autofree char *iri = NULL;
  g_autofree char *iri_out = NULL;

  loop = g_main_loop_new (NULL, FALSE);

  VALENT_TEST_CHECK ("Widget can be constructed");
  page = g_object_new (VALENT_TYPE_CONTACT_PAGE,
                       "contacts", contacts,
                       NULL);

  VALENT_TEST_CHECK ("Widget can be realized");
  window = g_object_new (GTK_TYPE_WINDOW,
                         "child",          page,
                         "default-height", 480,
                         "default-width",  600,
                         NULL);
  g_object_add_weak_pointer (G_OBJECT (window), (gpointer)&window);

  gtk_window_present (GTK_WINDOW (window));
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (page,
                "contacts", &contacts_out,
                NULL);
  g_assert_true (contacts == contacts_out);

  gtk_window_destroy (GTK_WINDOW (window));
  valent_test_await_nullptr (&window);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/contact-page",
                   test_contact_page);

  return g_test_run ();
}

