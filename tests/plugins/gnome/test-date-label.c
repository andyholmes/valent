// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gdk/gdk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#include "valent-date-label.h"


static void
test_sms_date_label (void)
{
  static ValentDateFormat formats[] = {
    VALENT_DATE_FORMAT_ADAPTIVE,
    VALENT_DATE_FORMAT_ADAPTIVE_SHORT,
    VALENT_DATE_FORMAT_TIME,
  };

  for (size_t i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      GtkWidget *window;
      GtkWidget *label;
      int64_t date = valent_timestamp_ms ();
      int64_t date_out;
      unsigned int mode = formats[i];
      unsigned int mode_out;

      VALENT_TEST_CHECK ("Widget can be constructed");
      label = g_object_new (VALENT_TYPE_DATE_LABEL,
                            "date", date,
                            "mode", mode,
                            NULL);

      VALENT_TEST_CHECK ("GObject properties function correctly");
      g_object_get (label,
                    "date", &date_out,
                    "mode", &mode_out,
                    NULL);

      g_assert_cmpint (date, ==, date_out);
      g_assert_cmpuint (mode, ==, mode_out);

      VALENT_TEST_CHECK ("Widget can be realized");
      window = g_object_new (GTK_TYPE_WINDOW,
                             "child",          label,
                             "default-height", 480,
                             "default-width",  600,
                             NULL);

      gtk_window_present (GTK_WINDOW (window));
      gtk_window_destroy (GTK_WINDOW (window));
    }
}

int
main (int   argc,
      char *argv[])
{
  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/date-label",
                   test_sms_date_label);

  return g_test_run ();
}

