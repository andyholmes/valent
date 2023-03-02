// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"
#include "valent-mpris-adapter.h"
#include "valent-mpris-impl.h"
#include "valent-mpris-player.h"


typedef struct
{
  ValentMedia       *media;
  ValentMediaPlayer *player;
  GMainLoop         *loop;
  gpointer           data;
  unsigned int       state;
} MprisComponentFixture;


static gboolean
timeout_cb (gpointer data)
{
  MprisComponentFixture *fixture = data;

  g_main_loop_quit (fixture->loop);

  return G_SOURCE_REMOVE;
}

static void
mpris_adapter_fixture_set_up (MprisComponentFixture *fixture,
                              gconstpointer          user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_test_mock_settings ("media");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->media = valent_media_get_default ();

  /* Wait just a tick to avoid a strange race condition */
  g_timeout_add (1, timeout_cb, fixture);
  g_main_loop_run (fixture->loop);
}

static void
mpris_adapter_fixture_tear_down (MprisComponentFixture *fixture,
                                 gconstpointer          user_data)
{
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_object (&fixture->player);

  v_assert_finalize_object (fixture->media);
}

static void
on_players_changed (ValentMedia           *media,
                    unsigned int           position,
                    unsigned int           removed,
                    unsigned int           added,
                    MprisComponentFixture *fixture)
{
  if (added == 1)
    fixture->player = g_list_model_get_item (G_LIST_MODEL (media), position);

  if (removed == 1)
    g_clear_object (&fixture->player);

  g_main_loop_quit (fixture->loop);
}

static void
on_player_notify (ValentMediaPlayer     *player,
                  GParamSpec            *pspec,
                  MprisComponentFixture *fixture)
{
  fixture->data = player;
  g_main_loop_quit (fixture->loop);
}

static void
valent_mpris_impl_export_full_cb (ValentMPRISImpl       *impl,
                                  GAsyncResult          *result,
                                  MprisComponentFixture *fixture)
{
  GError *error = NULL;

  valent_mpris_impl_export_finish (impl, result, &error);
  g_assert_no_error (error);
}

static void
test_mpris_component_adapter (MprisComponentFixture *fixture,
                              gconstpointer          user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;

  g_signal_connect (fixture->media,
                    "items-changed",
                    G_CALLBACK (on_players_changed),
                    fixture);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  impl = valent_mpris_impl_new (player);

  /* Adds exported players */
  valent_mpris_impl_export_full (impl,
                                 "org.mpris.MediaPlayer2.Test",
                                 NULL,
                                 (GAsyncReadyCallback)valent_mpris_impl_export_full_cb,
                                 NULL);
  g_main_loop_run (fixture->loop);
  g_assert_true (VALENT_IS_MEDIA_PLAYER (fixture->player));

  /* Removes unexported players */
  valent_mpris_impl_unexport (impl);
  g_main_loop_run (fixture->loop);
  g_assert_null (fixture->player);

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
}

static void
test_mpris_component_player (MprisComponentFixture *fixture,
                             gconstpointer          user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;
  ValentMediaActions flags;
  ValentMediaRepeat repeat;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  double position;
  gboolean shuffle;

  /* Watch for the player */
  g_signal_connect (fixture->media,
                    "items-changed",
                    G_CALLBACK (on_players_changed),
                    fixture);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  impl = valent_mpris_impl_new (player);

  /* Adds exported players */
  valent_mpris_impl_export_full (impl,
                                 "org.mpris.MediaPlayer2.Test",
                                 NULL,
                                 (GAsyncReadyCallback)valent_mpris_impl_export_full_cb,
                                 NULL);
  g_main_loop_run (fixture->loop);
  g_assert_true (VALENT_IS_MEDIA_PLAYER (fixture->player));

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
  g_assert_cmpfloat (position, ==, 0.0);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat (volume, ==, 1.0);
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
  g_main_loop_run (fixture->loop);
  g_assert_true (fixture->data == fixture->player);
  fixture->data = NULL;

  valent_media_player_play_pause (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_true (fixture->data == fixture->player);
  fixture->data = NULL;

  valent_media_player_pause (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_true (fixture->data == fixture->player);
  fixture->data = NULL;

  valent_media_player_stop (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_true (fixture->data == fixture->player);
  fixture->data = NULL;

  valent_media_player_next (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_true (fixture->data == fixture->player);
  fixture->data = NULL;

  valent_media_player_previous (fixture->player);
  g_main_loop_run (fixture->loop);
  g_assert_true (fixture->data == fixture->player);
  fixture->data = NULL;

  valent_media_player_seek (fixture->player, 1000);
  g_main_loop_run (fixture->loop);
  g_assert_true (fixture->data == fixture->player);
  fixture->data = NULL;

  valent_media_player_set_position (fixture->player, 5.0);
  g_main_loop_run (fixture->loop);
  /* g_assert_cmpfloat (valent_media_player_get_position (fixture->player), ==, 5.0); */
  /* fixture->data = NULL; */

  g_signal_handlers_disconnect_by_data (fixture->player, fixture);

  /* Remove Player */
  valent_mpris_impl_unexport (impl);
  g_main_loop_run (fixture->loop);
  g_assert_null (fixture->player);

  g_signal_handlers_disconnect_by_data (impl, fixture);
  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mpris/adapter",
              MprisComponentFixture, NULL,
              mpris_adapter_fixture_set_up,
              test_mpris_component_adapter,
              mpris_adapter_fixture_tear_down);

  g_test_add ("/plugins/mpris/player",
              MprisComponentFixture, NULL,
              mpris_adapter_fixture_set_up,
              test_mpris_component_player,
              mpris_adapter_fixture_tear_down);

  return g_test_run ();
}
