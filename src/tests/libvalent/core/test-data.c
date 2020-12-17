//#include <gutilsprivate.h>
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
test_data_get_gfile (DataFixture   *fixture,
                     gconstpointer  user_data)
{
  g_autoptr (GFile) cache_dir = NULL;
  g_autoptr (GFile) config_dir = NULL;
  g_autoptr (GFile) cache_file = NULL;
  g_autoptr (GFile) config_file = NULL;

  /* Cache file */
  cache_file = valent_data_get_cache_file (fixture->data, "filename.ext");
  g_assert_true (G_IS_FILE (cache_file));

  /* Config file */
  config_file = valent_data_get_config_file (fixture->data, "filename.ext");
  g_assert_true (G_IS_FILE (config_file));

  /* Cache file, dir should be created by get_cache_file() */
  cache_dir = g_file_get_parent (cache_file);
  g_assert_true (G_IS_FILE (cache_dir));
  g_assert_true (g_file_query_exists (cache_dir, NULL));

  /* Config file, dir should be created by get_config_file() */
  config_dir = g_file_get_parent (config_file);
  g_assert_true (G_IS_FILE (config_dir));
  g_assert_true (g_file_query_exists (config_dir, NULL));
}

static void
test_data_clear (DataFixture   *fixture,
                 gconstpointer  user_data)
{
  const char *cache_path;
  const char *config_path;

  /* Creates cache path on-demand */
  cache_path = valent_data_get_cache_path (fixture->data);
  g_assert_true (g_file_test (cache_path, G_FILE_TEST_IS_DIR));

  valent_data_clear_cache (fixture->data);
  g_assert_false (g_file_test (cache_path, G_FILE_TEST_IS_DIR));

  /* Sets cache file contents */
  config_path = valent_data_get_config_path (fixture->data);
  g_assert_true (g_file_test (config_path, G_FILE_TEST_IS_DIR));

  valent_data_clear_config (fixture->data);
  g_assert_false (g_file_test (config_path, G_FILE_TEST_IS_DIR));
}

gint
main (gint   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/core/data/basic",
              DataFixture, NULL,
              data_fixture_set_up,
              test_data_basic,
              data_fixture_tear_down);

  g_test_add ("/core/data/get-gfile",
              DataFixture, NULL,
              data_fixture_set_up,
              test_data_get_gfile,
              data_fixture_tear_down);

  g_test_add ("/core/data/clear",
              DataFixture, NULL,
              data_fixture_set_up,
              test_data_clear,
              data_fixture_tear_down);

  return g_test_run ();
}

