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
  gpointer            emitted;
  unsigned int        state;
} MediaComponentFixture;

static void
media_component_fixture_set_up (MediaComponentFixture *fixture,
                                gconstpointer          user_data)
{
  fixture->media = valent_media_get_default ();
  fixture->adapter = valent_test_await_adapter (fixture->media);
  fixture->player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);

  g_object_ref (fixture->adapter);
}

static void
media_component_fixture_tear_down (MediaComponentFixture *fixture,
                                   gconstpointer          user_data)
{
  v_assert_finalize_object (fixture->media);
  v_await_finalize_object (fixture->adapter);
  v_assert_finalize_object (fixture->player);

  valent_test_await_pending ();
}

static void
on_items_changed (GListModel            *list,
                  unsigned int           position,
                  unsigned int           removed,
                  unsigned int           added,
                  MediaComponentFixture *fixture)
{
  g_autoptr (GObject) item = NULL;

  // position 0 is the default mock store
  if (position == 0 && removed == 1)
    {
      fixture->emitter = NULL;
      fixture->emitted = NULL;
    }

  if (position == 0 && added == 1)
    {
      item = g_list_model_get_item (list, position);

      fixture->emitter = list;
      fixture->emitted = item;
    }
}

static void
on_player_notify (ValentMediaPlayer     *player,
                  GParamSpec            *pspec,
                  MediaComponentFixture *fixture)
{
  fixture->emitter = player;
}

static void
test_media_component_adapter (MediaComponentFixture *fixture,
                              gconstpointer          user_data)
{
  /* Signals */
  g_signal_connect (fixture->adapter,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    fixture);

  /* ::items-changed is emitted and the internal representation is updated */
  valent_media_adapter_player_added (fixture->adapter, fixture->player);
  g_assert_true (fixture->emitter == fixture->adapter);
  g_assert_true (fixture->emitted == fixture->player);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->adapter)), ==, 1);

  /* ::items-changed is emitted and the internal representation is updated */
  valent_media_adapter_player_removed (fixture->adapter, fixture->player);
  g_assert_null (fixture->emitter);
  g_assert_null (fixture->emitted);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->adapter)), ==, 0);

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_media_component_player (MediaComponentFixture *fixture,
                             gconstpointer          user_data)
{
  /* org.mpris.MediaPlayer2.Player */
  ValentMediaActions flags;
  ValentMediaState repeat;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  double position;
  gboolean shuffle;

  /* Add Player */
  g_signal_connect (fixture->adapter,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    fixture);

  valent_media_adapter_player_added (fixture->adapter, fixture->player);
  g_assert_true (fixture->emitter == fixture->adapter);
  g_assert_true (fixture->emitted == fixture->player);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->adapter)), ==, 1);

  /* Mock Player Properties */
  g_object_get (fixture->player,
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
  g_assert_cmpint (position, ==, 0);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat (volume, >=, 1.0);
  g_clear_pointer (&name, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  g_object_set (fixture->player,
                "shuffle", TRUE,
                "repeat",  VALENT_MEDIA_REPEAT_ALL,
                "volume",  0.5,
                NULL);

  /* Mock Player Methods */
  g_signal_connect (fixture->player,
                    "notify",
                    G_CALLBACK (on_player_notify),
                    fixture);

  valent_media_player_play (fixture->player);
  g_assert_true (fixture->emitter == fixture->player);
  fixture->emitter = NULL;

  valent_media_player_pause (fixture->player);
  g_assert_true (fixture->emitter == fixture->player);
  fixture->emitter = NULL;

  valent_media_player_stop (fixture->player);
  g_assert_true (fixture->emitter == fixture->player);
  fixture->emitter = NULL;

  valent_media_player_next (fixture->player);
  g_assert_true (fixture->emitter == fixture->player);
  fixture->emitter = NULL;

  valent_media_player_previous (fixture->player);
  g_assert_true (fixture->emitter == fixture->player);
  fixture->emitter = NULL;

  valent_media_player_seek (fixture->player, 1.0);
  g_assert_true (fixture->emitter == fixture->player);
  fixture->emitter = NULL;

  valent_media_player_set_position (fixture->player, 2.0);
  g_assert_cmpint (valent_media_player_get_position (fixture->player), ==, 2.0);
  fixture->emitter = NULL;

  g_signal_handlers_disconnect_by_data (fixture->player, fixture);

  /* Remove Player */
  valent_media_adapter_player_removed (fixture->adapter, fixture->player);
  g_assert_null (fixture->emitter);
  g_assert_null (fixture->emitted);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->adapter)), ==, 0);

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_media_component_self (MediaComponentFixture *fixture,
                           gconstpointer          user_data)
{
  g_autoptr (ValentMediaPlayer) player = NULL;
  unsigned int n_players = 0;

  /* Signals */
  g_signal_connect (fixture->media,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    fixture);

  /* ::items-changed propagates to ValentMedia */
  valent_media_adapter_player_added (fixture->adapter, fixture->player);
  g_assert_true (fixture->emitter == fixture->media);
  g_assert_true (fixture->emitted == fixture->player);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->media)), ==, 1);

  /* Test Component */
  n_players = g_list_model_get_n_items (G_LIST_MODEL (fixture->media));
  g_assert_cmpuint (n_players, ==, 1);

  player = g_list_model_get_item (G_LIST_MODEL (fixture->media), 0);
  g_assert_true (player == fixture->player);

  /* Players can be paused and unpaused */
  valent_media_player_play (fixture->player);
  g_assert_true (valent_media_player_get_state (player) == VALENT_MEDIA_STATE_PLAYING);
  valent_media_pause (fixture->media);
  g_assert_true (valent_media_player_get_state (player) == VALENT_MEDIA_STATE_PAUSED);
  valent_media_unpause (fixture->media);
  g_assert_true (valent_media_player_get_state (player) == VALENT_MEDIA_STATE_PLAYING);

  /* ::items-changed propagates to ValentMedia */
  valent_media_adapter_player_removed (fixture->adapter, fixture->player);
  g_assert_null (fixture->emitter);
  g_assert_null (fixture->emitted);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (fixture->media)), ==, 0);

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/media/adapter",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_adapter,
              media_component_fixture_tear_down);

  g_test_add ("/libvalent/media/player",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_player,
              media_component_fixture_tear_down);

  g_test_add ("/libvalent/media/self",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_self,
              media_component_fixture_tear_down);

  return g_test_run ();
}
