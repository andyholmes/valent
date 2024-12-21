// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>

#include <valent.h>
#include <libvalent-test.h>

#define VALENT_TYPE_MOCK_DATA_SOURCE (valent_mock_data_source_get_type())
G_DECLARE_FINAL_TYPE (ValentMockDataSource, valent_mock_data_source, VALENT, MOCK_DATA_SOURCE, ValentDataSource)

struct _ValentMockDataSource
{
  ValentDataSource parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockDataSource, valent_mock_data_source, VALENT_TYPE_DATA_SOURCE)

static void
valent_mock_data_source_class_init (ValentMockDataSourceClass *klass)
{
}

static void
valent_mock_data_source_init (ValentMockDataSource *self)
{
}

static void
test_data_source_basic (void)
{
  g_autoptr (ValentDataSource) source = NULL;
  g_autofree char *source_mode = g_uuid_string_random ();
  g_autofree char *source_mode_out = NULL;

  VALENT_TEST_CHECK ("Object can be constructed");
  source = g_object_new (VALENT_TYPE_MOCK_DATA_SOURCE,
                         "identifier",  "test-device",
                         "source-mode", source_mode,
                         NULL);
  g_assert_true (VALENT_IS_DATA_SOURCE (source));

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (source,
                "source-mode", &source_mode_out,
                NULL);

  g_assert_cmpstr (source_mode, ==, source_mode_out);
  g_assert_cmpstr (valent_data_source_get_source_mode (source), ==, source_mode_out);
}

static void
test_data_source_files (void)
{
  g_autoptr (ValentDataSource) source = NULL;
  g_autofree char *source_mode = g_uuid_string_random ();
  g_autoptr (GFile) cache_file = NULL;
  g_autoptr (GFile) config_file = NULL;

  VALENT_TEST_CHECK ("Object can be constructed");
  source = g_object_new (VALENT_TYPE_MOCK_DATA_SOURCE,
                         "identifier", "test-device",
                         "source-mode", source_mode,
                         NULL);
  g_assert_true (VALENT_IS_DATA_SOURCE (source));

  VALENT_TEST_CHECK ("Creates cache on-demand");
  cache_file = valent_data_source_get_cache_file (source, ".");
  g_assert_true (G_IS_FILE (cache_file));
  g_assert_true (g_file_query_exists (cache_file, NULL));

  VALENT_TEST_CHECK ("Clears cache on request");
  valent_data_source_clear_cache (source);
  g_assert_false (g_file_query_exists (cache_file, NULL));

  g_clear_object (&cache_file);

  VALENT_TEST_CHECK ("Creates config on-demand");
  cache_file = valent_data_source_get_cache_file (source, ".");
  config_file = valent_data_source_get_config_file (source, ".");
  g_assert_true (G_IS_FILE (config_file));
  g_assert_true (g_file_query_exists (config_file, NULL));

  VALENT_TEST_CHECK ("Clears cache and config on request");
  valent_data_source_clear_data (source);
  g_assert_false (g_file_query_exists (cache_file, NULL));
  g_assert_false (g_file_query_exists (config_file, NULL));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/core/data-source/basic",
                   test_data_source_basic);
  g_test_add_func ("/libvalent/core/data-source/files",
                   test_data_source_files);

  return g_test_run ();
}

