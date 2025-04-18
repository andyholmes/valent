// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <libebook-contacts/libebook-contacts.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#include "valent-contact-row.h"


static void
test_sms_contact_row (void)
{
  GtkWidget *window, *list;
  GtkWidget *row;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (EContact) contact = NULL;
  g_autoptr (EContact) contact_out = NULL;
  g_autofree char *contact_medium = NULL;

  bytes = g_resources_lookup_data ("/tests/contact.vcf", 0, NULL);
  contact = e_contact_new_from_vcard (g_bytes_get_data (bytes, NULL));

  VALENT_TEST_CHECK ("Widget can be constructed");
  row = g_object_new (VALENT_TYPE_CONTACT_ROW,
                      "contact", contact,
                      NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_set (row,
                "contact-medium", "123-456-7890",
                NULL);

  g_object_get (row,
                "contact",        &contact_out,
                "contact-medium", &contact_medium,
                NULL);

  g_assert_true (contact == contact_out);
  g_assert_cmpstr (contact_medium, ==, "123-456-7890");

  VALENT_TEST_CHECK ("Widget can be realized");
  list = gtk_list_box_new ();
  gtk_list_box_append (GTK_LIST_BOX (list), row);

  window = g_object_new (GTK_TYPE_WINDOW,
                         "child",          list,
                         "default-height", 480,
                         "default-width",  600,
                         NULL);
  gtk_window_present (GTK_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
test_sms_contact_list (void)
{
  GtkWidget *window, *list;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (EContact) contact = NULL;

  bytes = g_resources_lookup_data ("/tests/contact.vcf", 0, NULL);
  contact = e_contact_new_from_vcard_with_uid (g_bytes_get_data (bytes, NULL),
                                               "4077i252298cf8ded4bfe");

  VALENT_TEST_CHECK ("Row header function works correctly");
  list = gtk_list_box_new ();

  window = g_object_new (GTK_TYPE_WINDOW,
                         "child",          list,
                         "default-height", 480,
                         "default-width",  600,
                         NULL);
  gtk_window_present (GTK_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/contact-row",
                   test_sms_contact_row);
  g_test_add_func ("/plugins/gnome/contact-list",
                   test_sms_contact_list);

  return g_test_run ();
}

