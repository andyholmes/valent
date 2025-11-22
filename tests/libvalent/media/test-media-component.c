// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
} MediaComponentFixture;

static void
media_component_fixture_set_up (MediaComponentFixture *fixture,
                                gconstpointer          user_data)
{
  ValentMediaAdapter *adapter = NULL;
  g_autoptr (ValentMediaPlayer) player = NULL;

  adapter = valent_test_await_adapter (valent_media_get_default ());
  player = g_object_new (VALENT_TYPE_MEDIA_PLAYER, NULL);
  valent_media_adapter_player_added (adapter, player);
}

static void
media_component_fixture_tear_down (MediaComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  v_await_finalize_object (valent_media_get_default ());
}

static void
test_media_component_player (MediaComponentFixture *fixture,
                             gconstpointer          user_data)
{
  ValentMediaPlayer *player;
  ValentMediaActions flags;
  ValentMediaState repeat;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  double position;
  gboolean shuffle;

  VALENT_TEST_CHECK ("GObject can be constructed");
  player = g_object_new (VALENT_TYPE_MEDIA_PLAYER,
                         "position", 0.0,
                         "repeat",   VALENT_MEDIA_REPEAT_NONE,
                         "shuffle",  FALSE,
                         "volume",   1.0,
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (player,
                "name",     &name,
                "flags",    &flags,
                "metadata", &metadata,
                "position", &position,
                "repeat",   &repeat,
                "shuffle",  &shuffle,
                "state",    &state,
                "volume",   &volume,
                NULL);

  g_assert_cmpstr (name, ==, "Media Player");
  g_assert_cmpuint (flags, ==, VALENT_MEDIA_ACTION_NONE);
  g_assert_cmpfloat_with_epsilon (position, 0.0, DBL_EPSILON);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat_with_epsilon (volume, 1.0, DBL_EPSILON);
  g_clear_pointer (&name, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  VALENT_TEST_CHECK ("Player `set_position()` method works correctly");
  g_object_set (player, "position", 0.0, NULL);
  g_assert_cmpfloat_with_epsilon (valent_media_player_get_position (player), 0.0, DBL_EPSILON);

  VALENT_TEST_CHECK ("Player `set_repeat()` method works correctly");
  g_object_set (player, "repeat", VALENT_MEDIA_REPEAT_NONE, NULL);
  g_assert_cmpuint (valent_media_player_get_repeat (player), ==, VALENT_MEDIA_REPEAT_NONE);

  VALENT_TEST_CHECK ("Player `set_shuffle()` method works correctly");
  g_object_set (player, "shuffle", FALSE, NULL);
  g_assert_false (valent_media_player_get_shuffle (player));

  VALENT_TEST_CHECK ("Player `set_volume()` method works correctly");
  g_object_set (player, "volume", 1.0, NULL);
  g_assert_cmpfloat_with_epsilon (valent_media_player_get_volume (player), 1.0, 0.01);

  VALENT_TEST_CHECK ("Player `play()` method works correctly");
  valent_media_player_play (player);

  VALENT_TEST_CHECK ("Player `pause()` method works correctly");
  valent_media_player_pause (player);

  VALENT_TEST_CHECK ("Player `stop()` method works correctly");
  valent_media_player_stop (player);

  VALENT_TEST_CHECK ("Player `next()` method works correctly");
  valent_media_player_next (player);

  VALENT_TEST_CHECK ("Player `previous()` method works correctly");
  valent_media_player_previous (player);

  VALENT_TEST_CHECK ("Player `seek()` method works correctly");
  valent_media_player_seek (player, 1000);

  v_await_finalize_object (player);
}

static void
test_media_component_adapter (MediaComponentFixture *fixture,
                              gconstpointer          user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (GObject) adapter = NULL;
  ValentMediaPlayer *player;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");
  player = g_object_new (VALENT_TYPE_MEDIA_PLAYER, NULL);

  VALENT_TEST_CHECK ("Adapter can be constructed");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_MEDIA_ADAPTER,
                                          "iri",     "urn:valent:media:mock",
                                          "parent",  NULL,
                                          "context", context,
                                          NULL);

  VALENT_TEST_CHECK ("Adapter can export players");
  valent_media_adapter_player_added (VALENT_MEDIA_ADAPTER (adapter), player);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), ==, n_items + 1);

  VALENT_TEST_CHECK ("Adapter implements GListModel correctly");
  g_assert_true (G_IS_LIST_MODEL (adapter));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (adapter)) == VALENT_TYPE_MEDIA_PLAYER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (adapter));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (adapter), i);
      g_assert_true (VALENT_IS_MEDIA_PLAYER (item));
    }

  VALENT_TEST_CHECK ("Adapter can unexport players");
  valent_media_adapter_player_removed (VALENT_MEDIA_ADAPTER (adapter), player);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (adapter)), ==, n_items - 1);

  v_await_finalize_object (player);
}

static void
test_media_component_self (MediaComponentFixture *fixture,
                           gconstpointer          user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  ValentMedia *media = valent_media_get_default ();
  g_autoptr (GObject) adapter = NULL;
  ValentMediaPlayer *player;
  unsigned int n_items = 0;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");
  player = g_object_new (VALENT_TYPE_MEDIA_PLAYER, NULL);

  VALENT_TEST_CHECK ("Component implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (media));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (media)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (media)) == VALENT_TYPE_MEDIA_ADAPTER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (media));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = NULL;

      item = g_list_model_get_item (G_LIST_MODEL (media), i);
      g_assert_true (VALENT_IS_MEDIA_ADAPTER (item));
    }

  VALENT_TEST_CHECK ("Component can export adapters");
  adapter = peas_engine_create_extension (engine,
                                          plugin_info,
                                          VALENT_TYPE_MEDIA_ADAPTER,
                                          "iri",     "urn:valent:media:remote",
                                          "parent",  NULL,
                                          "context", context,
                                          NULL);

  valent_component_export_adapter (VALENT_COMPONENT (media),
                                   VALENT_EXTENSION (adapter));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (media)), ==, n_items + 1);

  VALENT_TEST_CHECK ("Component can unexport adapters");
  valent_component_unexport_adapter (VALENT_COMPONENT (media),
                                     VALENT_EXTENSION (adapter));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (media)), ==, n_items);

  VALENT_TEST_CHECK ("Component can export players");
  valent_media_export_player (media, player);

  VALENT_TEST_CHECK ("Component can unexport players");
  valent_media_unexport_player (media, player);

  v_await_finalize_object (player);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/media/player",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_player,
              media_component_fixture_tear_down);

  g_test_add ("/libvalent/media/adapter",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_adapter,
              media_component_fixture_tear_down);

  g_test_add ("/libvalent/media/self",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_self,
              media_component_fixture_tear_down);

  return g_test_run ();
}
