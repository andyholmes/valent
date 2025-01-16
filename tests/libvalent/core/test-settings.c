// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>

#include <valent.h>
#include <libvalent-test.h>

#define VALENT_TYPE_SETTINGS_DATA_SOURCE (valent_settings_data_source_get_type())
G_DECLARE_FINAL_TYPE (ValentSettingsDataSource, valent_settings_data_source, VALENT, SETTINGS_DATA_SOURCE, ValentDataSource)

struct _ValentSettingsDataSource
{
  ValentDataSource parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentSettingsDataSource, valent_settings_data_source, VALENT_TYPE_DATA_SOURCE)

static void
valent_settings_data_source_class_init (ValentSettingsDataSourceClass *klass)
{
}

static void
valent_settings_data_source_init (ValentSettingsDataSource *self)
{
}

static void
test_settings_basic (void)
{
  g_autoptr (ValentDataSource) data_source = NULL;
  g_autoptr (ValentSettings) settings = NULL;
  g_autofree char *identifier = g_uuid_string_random ();

  data_source = g_object_new (VALENT_TYPE_SETTINGS_DATA_SOURCE,
                              "identifier", identifier,
                              NULL);
  g_assert_true (VALENT_IS_DATA_SOURCE (data_source));

  VALENT_TEST_CHECK ("Object can be constructed");
  settings = g_object_new (VALENT_TYPE_SETTINGS,
                           "data-source", data_source,
                           "schema-id",   "ca.andyholmes.Valent.Device",
                           "path",        "/ca/andyholmes/valent/",
                           NULL);
  g_assert_true (VALENT_IS_SETTINGS (settings));

  /* VALENT_TEST_CHECK ("GObject properties function correctly"); */
  /* g_object_get (source, */
  /*               "source-mode", &source_mode_out, */
  /*               NULL); */

  /* g_assert_cmpstr (source_mode, ==, source_mode_out); */
  /* g_assert_cmpstr (valent_data_source_get_source_mode (source), ==, source_mode_out); */
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/core/settings/basic",
                   test_settings_basic);

  return g_test_run ();
}

