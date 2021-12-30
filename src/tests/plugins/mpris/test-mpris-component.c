// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-media.h>
#include <libvalent-test.h>

#include "test-mpris-common.h"
#include "valent-mpris-player.h"
#include "valent-mpris-player-provider.h"
#include "valent-mpris-remote.h"


typedef struct
{
  GMainLoop         *loop;
  ValentMediaPlayer *player;
  gpointer           data;
  unsigned int       state;
} MprisComponentFixture;


static void
mpris_provider_fixture_set_up (MprisComponentFixture *fixture,
                               gconstpointer          user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_component_new_settings ("media", "mock");
  g_settings_set_boolean (settings, "enabled", FALSE);

  /* Wait for extensions to load */
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
mpris_provider_fixture_tear_down (MprisComponentFixture *fixture,
                                  gconstpointer          user_data)
{
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_object (&fixture->player);

  // TODO: finalize ValentMedia to finalize ValentMPRISPlayerProvider
  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

static void
on_player_added (ValentMedia           *media,
                 ValentMediaPlayer     *player,
                 MprisComponentFixture *fixture)
{
  fixture->player = player;
  g_main_loop_quit (fixture->loop);
}

static void
on_player_removed (ValentMedia           *media,
                   ValentMediaPlayer     *player,
                   MprisComponentFixture *fixture)
{
  fixture->player = NULL;
  g_main_loop_quit (fixture->loop);
}

static void
on_player_method (ValentMediaPlayer     *player,
                  const char            *method_name,
                  GVariant              *args,
                  MprisComponentFixture *fixture)
{
  fixture->data = g_strdup (method_name);
  g_main_loop_quit (fixture->loop);
}

static void
test_mpris_component_provider (MprisComponentFixture *fixture,
                               gconstpointer          user_data)
{
  ValentMedia *media;
  g_autoptr (ValentMprisRemote) remote = NULL;

  media = valent_media_get_default ();
  g_signal_connect (media,
                    "player-added",
                    G_CALLBACK (on_player_added),
                    fixture);
  g_signal_connect (media,
                    "player-removed",
                    G_CALLBACK (on_player_removed),
                    fixture);

  /* Adds exported players */
  remote = valent_test_mpris_get_remote ();
  g_main_loop_run (fixture->loop);
  g_assert_true (VALENT_IS_MEDIA_PLAYER (fixture->player));

  /* Removes unexported players */
  valent_mpris_remote_unexport (remote);
  g_main_loop_run (fixture->loop);
  g_assert_null (fixture->player);

  g_signal_handlers_disconnect_by_data (media, fixture);
}

static void
test_mpris_component_player (MprisComponentFixture *fixture,
                             gconstpointer          user_data)
{
  ValentMedia *media;
  g_autoptr (ValentMprisRemote) remote = NULL;
  ValentMediaActions flags;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  gint64 position;

  /* Watch for the player */
  media = valent_media_get_default ();
  g_signal_connect (media,
                    "player-added",
                    G_CALLBACK (on_player_added),
                    fixture);
  g_signal_connect (media,
                    "player-removed",
                    G_CALLBACK (on_player_removed),
                    fixture);

  /* Add player */
  remote = valent_test_mpris_get_remote ();
  g_main_loop_run (fixture->loop);
  g_assert_true (VALENT_IS_MEDIA_PLAYER (fixture->player));

  /* Test Player Properties */
  g_object_get (fixture->player,
                "flags",    &flags,
                "state",    &state,
                "volume",   &volume,
                "name",     &name,
                "metadata", &metadata,
                "position", &position,
                NULL);

  g_assert_cmpuint (flags, ==, VALENT_MEDIA_ACTION_NONE);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat (volume, ==, 1.0);

  g_assert_cmpstr (name, ==, "Test Player");
  g_free (name);
  g_assert_cmpint (position, ==, 0);

  g_object_set (fixture->player,
                "state",  (VALENT_MEDIA_STATE_REPEAT_ALL |
                           VALENT_MEDIA_STATE_SHUFFLE),
                "volume", 1.0,
                NULL);

  /* Test Player Methods */
  g_signal_connect (remote,
                    "method-call",
                    G_CALLBACK (on_player_method),
                    fixture);

  valent_media_player_play (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "Play");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_play_pause (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "PlayPause");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_pause (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "Pause");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_stop (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "Stop");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_next (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "Next");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_previous (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "Previous");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_open_uri (fixture->player, "https://andyholmes.ca");
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "OpenUri");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_seek (fixture->player, 1000);
  g_main_loop_run (fixture->loop);
  g_assert_cmpstr (fixture->data, ==, "Seek");
  g_clear_pointer (&fixture->data, g_free);

  valent_media_player_set_position (fixture->player, "/dbus/path", 5);
  //g_assert_cmpint (valent_media_player_get_position (fixture->player), ==, 5);

  /* Remove Player */
  valent_mpris_remote_unexport (remote);
  g_main_loop_run (fixture->loop);
  g_assert_null (fixture->player);

  g_signal_handlers_disconnect_by_data (remote, fixture);
  g_signal_handlers_disconnect_by_data (media, fixture);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

#ifdef VALENT_TEST_DBUS
  g_test_add ("/plugins/mpris/provider",
              MprisComponentFixture, NULL,
              mpris_provider_fixture_set_up,
              test_mpris_component_provider,
              mpris_provider_fixture_tear_down);

  g_test_add ("/plugins/mpris/player",
              MprisComponentFixture, NULL,
              mpris_provider_fixture_set_up,
              test_mpris_component_player,
              mpris_provider_fixture_tear_down);
#endif

  return g_test_run ();
}
