// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>
#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "test-sms-common.h"
#include "valent-sms-utils.h"


struct
{
  const char *original;
  const char *normalized;
} numbers[] = {
  {"754-3010",         "7543010"},     // Local
  {"(541) 754-3010",   "5417543010"},  // Domestic
  {"+1-541-754-3010",  "15417543010"}, // International
  {"1-541-754-3010",   "15417543010"}, // International (US)
  {"001-541-754-3010", "15417543010"}  // International (EU)
};


static const char phone_vcard[] =
 "BEGIN:VCARD\n"
 "VERSION:2.1\n"
 "FN:Test Contact\n"
 "TEL;CELL:123-456-7890\n"
 "END:VCARD\n";


static void
test_sms_avatar_from_contact (void)
{
  GtkWidget *window;
  GtkWidget *avatar;
  GdkPaintable *paintable;
  g_autoptr (EContact) contact = NULL;
  g_autofree char *vcard = NULL;

  g_file_get_contents (TEST_DATA_DIR"/contact.vcf", &vcard, NULL, NULL);
  contact = e_contact_new_from_vcard (vcard);

  avatar = g_object_new (ADW_TYPE_AVATAR,
                         "size", 32,
                         NULL);

  valent_sms_avatar_from_contact (ADW_AVATAR (avatar), contact);
  paintable = adw_avatar_get_custom_image (ADW_AVATAR (avatar));
  g_assert_true (GDK_IS_PAINTABLE (paintable));

  /* Display */
  window = gtk_window_new ();
  gtk_window_set_child (GTK_WINDOW (window), avatar);

  gtk_window_present (GTK_WINDOW (window));
  gtk_window_destroy (GTK_WINDOW (window));
}

static void
dup_for_phone_cb (ValentContactStore *store,
                  GAsyncResult       *result,
                  GMainLoop          *loop)
{
  g_autoptr (EContact) contact = NULL;
  GError *error = NULL;

  contact = valent_sms_contact_from_phone_finish (store, result, &error);
  g_assert_no_error (error);

  g_assert_true (E_IS_CONTACT (contact));
  g_assert_cmpstr (e_contact_get_const (contact, E_CONTACT_UID), ==, "4077i252298cf8ded4bff");
  g_clear_object (&contact);

  g_main_loop_quit (loop);
}

static void
test_sms_contact_from_phone (void)
{
  g_autoptr (GMainLoop) loop = NULL;
  g_autoptr (ValentContactStore) store = NULL;

  loop = g_main_loop_new (NULL, FALSE);
  store = valent_test_contact_store_new ();

  /* Contacts can be queried by telephone number (Contact #2) */
  valent_sms_contact_from_phone (store,
                                 "+1-234-567-8912",
                                 NULL,
                                 (GAsyncReadyCallback)dup_for_phone_cb,
                                 loop);
  g_main_loop_run (loop);
}

static void
test_sms_phone_number (void)
{
  g_autoptr (EContact) contact = NULL;
  char *normalized;
  gboolean ret;

  /* Normalize & Compare */
  for (unsigned int i = 0; i < G_N_ELEMENTS (numbers); i++)
    {
      gboolean equal;

      normalized = valent_phone_number_normalize (numbers[i].original);
      g_assert_cmpstr (normalized, ==, numbers[i].normalized);
      g_free (normalized);

      if (i > 0)
        {
          equal = valent_phone_number_equal (numbers[i - 1].original,
                                             numbers[i].original);
          g_assert_true (equal);
        }
    }

  /* Test Contact */
  contact = e_contact_new_from_vcard_with_uid (phone_vcard, "test-contact");
  normalized = valent_phone_number_normalize ("123-456-7890");

  ret = valent_phone_number_of_contact (contact, normalized);
  g_assert_true (ret);
  g_free (normalized);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/sms/avatar-from-contact",
                   test_sms_avatar_from_contact);

  g_test_add_func ("/plugins/sms/contact-from-phone",
                   test_sms_contact_from_phone);

  g_test_add_func ("/plugins/sms/phone-number",
                   test_sms_phone_number);

  return g_test_run ();
}

