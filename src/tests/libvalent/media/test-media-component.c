// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-media.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"


typedef struct
{
  ValentMedia        *media;
  ValentMediaAdapter *adapter;
  ValentMediaPlayer  *player;
  gpointer            data;
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

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

static void
on_player_added (GObject               *object,
                 ValentMediaPlayer     *player,
                 MediaComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_player_changed (ValentMedia           *media,
                   ValentMediaPlayer     *player,
                   MediaComponentFixture *fixture)
{
  fixture->state = TRUE;
}

static void
on_player_seeked (ValentMedia           *media,
                  ValentMediaPlayer     *player,
                  gint64                 offset,
                  MediaComponentFixture *fixture)
{
  fixture->state = (offset == 1000);
}

static void
on_player_removed (GObject               *object,
                   ValentMediaPlayer     *player,
                   MediaComponentFixture *fixture)
{
  fixture->data = object;
}

static void
on_player_method (ValentMediaPlayer     *player,
                  const char            *method_name,
                  GVariant              *args,
                  MediaComponentFixture *fixture)
{
  fixture->data = g_strdup (method_name);
}

static void
test_media_component_adapter (MediaComponentFixture *fixture,
                              gconstpointer          user_data)
{
  g_autoptr (GPtrArray) players = NULL;
  PeasPluginInfo *plugin_info;

  /* Properties */
  g_object_get (fixture->adapter,
                "plugin-info", &plugin_info,
                NULL);

  g_assert_nonnull (plugin_info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, plugin_info);

  /* Signals */
  g_signal_connect (fixture->adapter,
                    "player-added",
                    G_CALLBACK (on_player_added),
                    fixture);
  valent_media_adapter_emit_player_added (fixture->adapter, fixture->player);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  players = valent_media_adapter_get_players (fixture->adapter);
  g_assert_cmpint (players->len, ==, 1);

  g_signal_connect (fixture->adapter,
                    "player-removed",
                    G_CALLBACK (on_player_removed),
                    fixture);
  valent_media_adapter_emit_player_removed (fixture->adapter, fixture->player);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

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
  gint64 position;
  gboolean shuffle;

  /* Add Player */
  g_signal_connect (fixture->adapter,
                    "player-added",
                    G_CALLBACK (on_player_added),
                    fixture);
  valent_media_adapter_emit_player_added (fixture->adapter, fixture->player);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  /* Test Player Properties */
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

  g_assert_cmpstr (name, ==, "Media Player");
  g_assert_cmpuint (flags, ==, VALENT_MEDIA_ACTION_NONE);
  g_assert_cmpint (position, ==, 0);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat (volume, ==, 0.0);
  g_clear_pointer (&name, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  g_object_set (fixture->player,
                "shuffle", TRUE,
                "repeat",  VALENT_MEDIA_REPEAT_ALL,
                "volume",  1.0,
                NULL);

  /* Test Player Methods */
  g_signal_connect (fixture->player,
                    "player-method",
                    G_CALLBACK (on_player_method),
                    fixture);

  valent_media_player_play (fixture->player);
  g_assert_cmpstr (fixture->data, ==, "Play");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_play_pause (fixture->player);
  g_assert_cmpstr (fixture->data, ==, "PlayPause");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_pause (fixture->player);
  g_assert_cmpstr (fixture->data, ==, "Pause");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_stop (fixture->player);
  g_assert_cmpstr (fixture->data, ==, "Stop");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_next (fixture->player);
  g_assert_cmpstr (fixture->data, ==, "Next");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_previous (fixture->player);
  g_assert_cmpstr (fixture->data, ==, "Previous");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_seek (fixture->player, 1000);
  g_assert_cmpstr (fixture->data, ==, "Seek");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_set_position (fixture->player, 5);
  g_assert_cmpint (valent_media_player_get_position (fixture->player), ==, 5);

  /* Test signal propagation */
  g_signal_connect (fixture->media,
                    "player-changed",
                    G_CALLBACK (on_player_changed),
                    fixture);
  valent_media_player_emit_changed (fixture->player);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  g_signal_connect (fixture->media,
                    "player-seeked",
                    G_CALLBACK (on_player_seeked),
                    fixture);
  valent_media_player_emit_seeked (fixture->player, 1000);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  /* Remove Player */
  g_signal_connect (fixture->adapter,
                    "player-removed",
                    G_CALLBACK (on_player_removed),
                    fixture);
  valent_media_adapter_emit_player_removed (fixture->adapter, fixture->player);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_media_component_self (MediaComponentFixture *fixture,
                           gconstpointer          user_data)
{
  g_autoptr (GPtrArray) players = NULL;
  ValentMediaPlayer *player;

  /* Add Player */
  g_signal_connect (fixture->media,
                    "player-added",
                    G_CALLBACK (on_player_added),
                    fixture);
  valent_media_adapter_emit_player_added (fixture->adapter, fixture->player);
  g_assert_true (fixture->data == fixture->media);
  fixture->data = NULL;

  /* Test Component */
  players = valent_media_get_players (fixture->media);
  g_assert_cmpuint (players->len, ==, 1);

  player = valent_media_get_player_by_name (fixture->media, "Media Player");
  g_assert_nonnull (player);

  g_assert_true (player == fixture->player);
  g_assert_true (player == g_ptr_array_index (players, 0));

  valent_media_player_set_state (fixture->player, VALENT_MEDIA_STATE_PLAYING);
  valent_media_pause (fixture->media);
  g_assert_false (valent_media_player_is_playing (fixture->player));
  valent_media_unpause (fixture->media);
  g_assert_true (valent_media_player_is_playing (fixture->player));

  /* Remove Player */
  g_signal_connect (fixture->media,
                    "player-removed",
                    G_CALLBACK (on_player_removed),
                    fixture);
  valent_media_adapter_emit_player_removed (fixture->adapter, fixture->player);
  g_assert_true (fixture->data == fixture->media);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/components/media/adapter",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_adapter,
              media_component_fixture_tear_down);

  g_test_add ("/components/media/player",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_player,
              media_component_fixture_tear_down);

  g_test_add ("/components/media/self",
              MediaComponentFixture, NULL,
              media_component_fixture_set_up,
              test_media_component_self,
              media_component_fixture_tear_down);

  return g_test_run ();
}
