// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-sms-common.h"
#include "valent-ui-utils-private.h"


static void
test_sms_avatar_from_contact (void)
{
  GtkWidget *window;
  GtkWidget *avatar;
  GdkPaintable *paintable;
  g_autoptr (EContact) contact = NULL;
  g_autoptr (GBytes) bytes = NULL;

  bytes = g_resources_lookup_data ("/tests/contact.vcf", 0, NULL);
  contact = e_contact_new_from_vcard (g_bytes_get_data (bytes, NULL));

  avatar = g_object_new (ADW_TYPE_AVATAR,
                         "size", 32,
                         NULL);

  VALENT_TEST_CHECK ("Function `valent_sms_avatar_from_contact()` populate an "
                     "`AdwAvatar` from an `EContact`.");
  valent_sms_avatar_from_contact (ADW_AVATAR (avatar), contact);
  paintable = adw_avatar_get_custom_image (ADW_AVATAR (avatar));
  g_assert_true (GDK_IS_PAINTABLE (paintable));

  VALENT_TEST_CHECK ("The resulting `AdwAvatar` can be realized.");
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

  contact = valent_contact_store_lookup_contact_finish (store, result, &error);
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
  VALENT_TEST_CHECK ("Function `valent_contact_store_lookup_contact()` can query "
                     "`EContact`s by phone number.");
  valent_contact_store_lookup_contact (store,
                                       "+1-234-567-8912",
                                       NULL,
                                       (GAsyncReadyCallback)dup_for_phone_cb,
                                       loop);
  g_main_loop_run (loop);
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

  return g_test_run ();
}

