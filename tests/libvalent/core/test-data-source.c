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
  g_autofree char *source_mode_out = NULL;

  VALENT_TEST_CHECK ("Object can be constructed");
  source = g_object_new (VALENT_TYPE_MOCK_DATA_SOURCE,
                         "identifier", "test-device",
                         NULL);
  g_assert_true (VALENT_IS_DATA_SOURCE (source));

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (source,
                "source-mode", &source_mode_out,
                NULL);
  g_assert_null (source_mode_out);
}

static void
test_data_source_files (void)
{
  g_autoptr (ValentDataSource) source = NULL;
  GFile *cache_dir = NULL;
  g_autoptr (GFile) cache_files = NULL;
  GFile *config_dir = NULL;
  g_autoptr (GFile) cache_file = NULL;
  g_autoptr (GFile) config_file = NULL;
  g_autofree char * cache_filename = g_uuid_string_random ();
  g_autofree char * config_filename = g_uuid_string_random ();

  VALENT_TEST_CHECK ("Object can be constructed");
  source = g_object_new (VALENT_TYPE_MOCK_DATA_SOURCE,
                         "identifier", "test-device",
                         NULL);
  g_assert_true (VALENT_IS_DATA_SOURCE (source));

  VALENT_TEST_CHECK ("Initializes directories");
  cache_dir = valent_data_source_get_cache_directory (source);
  cache_files = g_file_get_child (cache_dir, "files");
  config_dir = valent_data_source_get_config_directory (source);
  g_assert_true (g_file_query_exists (cache_dir, NULL));
  g_assert_true (g_file_query_exists (cache_files, NULL));
  g_assert_true (g_file_query_exists (config_dir, NULL));

  VALENT_TEST_CHECK ("Creates cache and config file objects");
  cache_file = valent_data_source_get_cache_file (source, cache_filename);
  g_assert_nonnull (g_strrstr (g_file_peek_path (cache_file), cache_filename));
  config_file = valent_data_source_get_config_file (source, config_filename);
  g_assert_nonnull (g_strrstr (g_file_peek_path (config_file), config_filename));

  VALENT_TEST_CHECK ("Clears cache on request");
  valent_data_source_clear_cache (source);
  g_assert_false (g_file_query_exists (cache_dir, NULL));

  VALENT_TEST_CHECK ("Clears cache and config on request");
  valent_data_source_clear_data (source);
  g_assert_false (g_file_query_exists (cache_dir, NULL));
  g_assert_false (g_file_query_exists (config_dir, NULL));
}

static void
valent_data_source_get_sparql_connection_cb (ValentDataSource         *data_source,
                                             GAsyncResult             *result,
                                             TrackerSparqlConnection **connection)
{
  GError *error = NULL;

  *connection = valent_data_source_get_sparql_connection_finish (data_source,
                                                                 result,
                                                                 &error);
  g_assert_no_error (error);
}

static void
test_data_source_sparql (void)
{
  g_autoptr (ValentDataSource) data_source = NULL;
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  GError *error = NULL;

  VALENT_TEST_CHECK ("Object can be constructed");
  data_source = g_object_new (VALENT_TYPE_MOCK_DATA_SOURCE,
                              "identifier", "test-device",
                              NULL);
  g_assert_true (VALENT_IS_DATA_SOURCE (data_source));

  VALENT_TEST_CHECK ("Object can be constructed");
  valent_data_source_get_sparql_connection (data_source,
                                            NULL,
                                            (GAsyncReadyCallback)valent_data_source_get_sparql_connection_cb,
                                            &connection);
  valent_test_await_pointer (&connection);

  g_assert_true (TRACKER_IS_SPARQL_CONNECTION (connection));
  tracker_sparql_connection_close (connection);
  g_clear_object (&connection);

  VALENT_TEST_CHECK ("Object can be constructed");
  connection = valent_data_source_get_sparql_connection_sync (data_source,
                                                              NULL,
                                                              &error);
  g_assert_no_error (error);

  g_assert_true (TRACKER_IS_SPARQL_CONNECTION (connection));
  tracker_sparql_connection_close (connection);
  g_clear_object (&connection);
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
  g_test_add_func ("/libvalent/core/data-source/sparql",
                   test_data_source_sparql);

  return g_test_run ();
}

