// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-contact-row.h"


static void
test_sms_contact_row (void)
{
  GtkWidget *window, *list;
  GtkWidget *row;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (EContact) contact = NULL;
  g_autoptr (EContact) contact_out = NULL;
  g_autofree char *contact_name = NULL;
  g_autofree char *contact_addr = NULL;

  bytes = g_resources_lookup_data ("/tests/contact.vcf", 0, NULL);
  contact = e_contact_new_from_vcard (g_bytes_get_data (bytes, NULL));

  /* Construction */
  row = valent_contact_row_new (contact);

  /* Properties */
  g_object_set (row,
                "contact-name",    "Test Contact",
                "contact-address", "123-456-7890",
                NULL);

  g_object_get (row,
                "contact",         &contact_out,
                "contact-name",    &contact_name,
                "contact-address", &contact_addr,
                NULL);

  g_assert_true (contact == contact_out);
  g_assert_cmpstr (contact_name, ==, "Test Contact");
  g_assert_cmpstr (contact_addr, ==, "123-456-7890");

  /* Display */
  list = gtk_list_box_new ();
  gtk_list_box_append (GTK_LIST_BOX (list), row);

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), list);

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

  /* Display */
  list = gtk_list_box_new ();
  gtk_list_box_set_header_func (GTK_LIST_BOX (list),
                                valent_contact_row_header_func,
                                NULL, NULL);
  valent_list_add_contact (GTK_LIST_BOX (list), contact);

  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), list);

  gtk_window_present (GTK_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/contact-row",
                   test_sms_contact_row);
  g_test_add_func ("/plugins/sms/contact-list",
                   test_sms_contact_list);

  return g_test_run ();
}

