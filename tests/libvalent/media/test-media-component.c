// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"


typedef struct
{
  ValentMedia        *media;
  ValentMediaAdapter *adapter;
  ValentMediaPlayer  *player;
  gpointer            emitter;
} MediaComponentFixture;

static void
media_component_fixture_set_up (MediaComponentFixture *fixture,
                                gconstpointer          user_data)
{
  fixture->media = valent_media_get_default ();
  fixture->adapter = valent_test_await_adapter (fixture->media);
  fixture->player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  valent_media_adapter_player_added (fixture->adapter, fixture->player);

  g_object_ref (fixture->adapter);
}

static void
media_component_fixture_tear_down (MediaComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  v_await_finalize_object (fixture->media);
  v_await_finalize_object (fixture->adapter);
  v_await_finalize_object (fixture->player);

  valent_test_await_pending ();
}

static void
on_player_notify (ValentMediaPlayer     *player,
                  GParamSpec            *pspec,
                  MediaComponentFixture *fixture)
{
  fixture->emitter = player;
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
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER,
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

  g_assert_cmpstr (name, ==, "Mock Player");
  g_assert_cmpuint (flags, ==, VALENT_MEDIA_ACTION_NONE);
  g_assert_cmpfloat_with_epsilon (position, 0.0, DBL_EPSILON);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat_with_epsilon (volume, 1.0, DBL_EPSILON);
  g_clear_pointer (&name, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  g_object_set (player,
                "repeat",  VALENT_MEDIA_REPEAT_ALL,
                "shuffle", TRUE,
                "volume",  0.5,
                NULL);

  g_assert_cmpuint (valent_media_player_get_repeat (player), ==, VALENT_MEDIA_REPEAT_ALL);
  g_assert_true (valent_media_player_get_shuffle (player));
  g_assert_cmpfloat_with_epsilon (valent_media_player_get_volume (player), 0.5, DBL_EPSILON);

  /* Mock Player Methods */
  g_signal_connect (player,
                    "notify",
                    G_CALLBACK (on_player_notify),
                    fixture);

  valent_media_player_play (player);
  g_assert_true (fixture->emitter == player);
  fixture->emitter = NULL;

  valent_media_player_pause (player);
  g_assert_true (fixture->emitter == player);
  fixture->emitter = NULL;

  valent_media_player_stop (player);
  g_assert_true (fixture->emitter == player);
  fixture->emitter = NULL;

  valent_media_player_next (player);
  g_assert_true (fixture->emitter == player);
  fixture->emitter = NULL;

  valent_media_player_previous (player);
  g_assert_true (fixture->emitter == player);
  fixture->emitter = NULL;

  valent_media_player_seek (player, 1.0);
  g_assert_true (fixture->emitter == player);
  fixture->emitter = NULL;

  valent_media_player_set_position (player, 2.0);
  g_assert_cmpfloat_with_epsilon (valent_media_player_get_position (player), 2.0, DBL_EPSILON);
  fixture->emitter = NULL;

  g_signal_handlers_disconnect_by_data (player, fixture);
  v_await_finalize_object (player);
}

static void
test_media_component_adapter (MediaComponentFixture *fixture,
                              gconstpointer          user_data)
{
  ValentMedia *media = valent_media_get_default ();
  GListModel *list = G_LIST_MODEL (fixture->adapter);
  ValentMediaPlayer *player;
  unsigned int n_items = 0;

  VALENT_TEST_CHECK ("Adapter implements GListModel correctly");
  g_assert_true (G_IS_LIST_MODEL (list));
  g_assert_cmpuint (g_list_model_get_n_items (list), >, 0);
  g_assert_true (g_list_model_get_item_type (list) == VALENT_TYPE_MEDIA_PLAYER);

  n_items = g_list_model_get_n_items (list);
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GObject) item = NULL;

      item = g_list_model_get_item (list, i);
      g_assert_true (VALENT_IS_MEDIA_PLAYER (item));
    }

  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);

  valent_media_adapter_player_added (fixture->adapter, player);
  valent_media_export_player (media, player);
  valent_media_unexport_player (media, player);
  valent_media_adapter_player_removed (fixture->adapter, player);

  v_await_finalize_object (player);
}

static void
test_media_component_self (MediaComponentFixture *fixture,
                           gconstpointer          user_data)
{
  ValentMedia *media = valent_media_get_default ();
  ValentMediaPlayer *player;
  unsigned int n_items = 0;

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

  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);

  valent_media_adapter_player_added (fixture->adapter, player);
  valent_media_export_player (media, player);
  valent_media_unexport_player (media, player);
  valent_media_adapter_player_removed (fixture->adapter, player);

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
