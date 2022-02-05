// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <libvalent-core.h>
#include <libvalent-test.h>
#include <libvalent-ui.h>

#include "valent-sms-utils.h"


static void
test_sms_utils (void)
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

int
main (int   argc,
      char *argv[])
{
  /* TODO: skip using valent_test_ui_init() because gdk-pixbuf fails to load
   *       JPEG images with %G_TEST_OPTION_ISOLATE_DIRS set, even if
   *       `GDK_PIXBUF_MODULE_FILE` is set correctly in the env.
   */
  g_test_init (&argc, &argv, NULL);
  gtk_init ();
  adw_init ();

  g_test_add_func ("/plugins/sms/utils",
                   test_sms_utils);

  return g_test_run ();
}

