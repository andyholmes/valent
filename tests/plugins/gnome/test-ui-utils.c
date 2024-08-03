// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <adwaita.h>
#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

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

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/utils/avatar-from-contact",
                   test_sms_avatar_from_contact);

  return g_test_run ();
}

