// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>

#include <libvalent-core.h>
#include <libvalent-test.h>


static gboolean ready = FALSE;


typedef struct
{
  ValentData *data;
} DataFixture;

static void
data_fixture_set_up (DataFixture   *fixture,
                     gconstpointer  user_data)
{
  fixture->data = g_object_new (VALENT_TYPE_DATA,
                                "context", "test-device",
                                NULL);
}

static void
data_fixture_tear_down (DataFixture   *fixture,
                        gconstpointer  user_data)
{
  g_clear_object (&fixture->data);
  ready = FALSE;
}

static void
test_data_basic (DataFixture   *fixture,
                 gconstpointer  user_data)
{
  const char *context;

  context = valent_data_get_context (fixture->data);
  g_assert_nonnull (context);
}

static void
test_data_directories (DataFixture   *fixture,
                       gconstpointer  user_data)
{
  const char *cache_path;
  const char *config_path;
  const char *data_path;
  g_autoptr (GFile) cache_file = NULL;
  g_autoptr (GFile) config_file = NULL;
  g_autoptr (GFile) data_file = NULL;

  /* Creates cache path on-demand and clears contents */
  cache_path = valent_data_get_cache_path (fixture->data);
  g_assert_true (g_file_test (cache_path, G_FILE_TEST_IS_DIR));

  valent_data_clear_cache (fixture->data);
  g_assert_false (g_file_test (cache_path, G_FILE_TEST_IS_DIR));

  /* Creates config, data path on-demand and clears contents */
  config_path = valent_data_get_config_path (fixture->data);
  g_assert_true (g_file_test (config_path, G_FILE_TEST_IS_DIR));

  data_path = valent_data_get_data_path (fixture->data);
  g_assert_true (g_file_test (data_path, G_FILE_TEST_IS_DIR));

  valent_data_clear_data (fixture->data);
  g_assert_false (g_file_test (config_path, G_FILE_TEST_IS_DIR));
  g_assert_false (g_file_test (data_path, G_FILE_TEST_IS_DIR));

  /* Cache, Config, Data file */
  cache_file = valent_data_new_cache_file (fixture->data, "filename.ext");
  g_assert_true (G_IS_FILE (cache_file));

  config_file = valent_data_new_config_file (fixture->data, "filename.ext");
  g_assert_true (G_IS_FILE (config_file));

  data_file = valent_data_new_data_file (fixture->data, "filename.ext");
  g_assert_true (G_IS_FILE (data_file));

  /* Parent directories should be created by valent_data_new_*_file() */
  g_assert_true (g_file_test (cache_path, G_FILE_TEST_IS_DIR));
  g_assert_true (g_file_test (config_path, G_FILE_TEST_IS_DIR));
  g_assert_true (g_file_test (data_path, G_FILE_TEST_IS_DIR));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/core/data/basic",
              DataFixture, NULL,
              data_fixture_set_up,
              test_data_basic,
              data_fixture_tear_down);

  g_test_add ("/core/data/directories",
              DataFixture, NULL,
              data_fixture_set_up,
              test_data_directories,
              data_fixture_tear_down);

  return g_test_run ();
}

