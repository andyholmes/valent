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
  ValentMediaPlayer *export;
} MPRISAdapterFixture;


static void
mpris_adapter_fixture_set_up (MPRISAdapterFixture *fixture,
                              gconstpointer        user_data)
{
  g_autoptr (GSettings) settings = NULL;

  /* Disable the mock plugin */
  settings = valent_test_mock_settings ("media");
  g_settings_set_boolean (settings, "enabled", FALSE);

  fixture->media = valent_media_get_default ();
  fixture->export = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);

  /* Wait just a tick to avoid a strange race condition */
  valent_test_await_pending ();
}

static void
mpris_adapter_fixture_tear_down (MPRISAdapterFixture *fixture,
                                 gconstpointer        user_data)
{
  g_clear_object (&fixture->player);
  g_clear_object (&fixture->export);
  v_assert_finalize_object (fixture->media);
}

static void
on_players_changed (ValentMedia         *media,
                    unsigned int         position,
                    unsigned int         removed,
                    unsigned int         added,
                    MPRISAdapterFixture *fixture)
{
  if (added == 1)
    fixture->player = g_list_model_get_item (G_LIST_MODEL (media), position);

  if (removed == 1)
    g_clear_object (&fixture->player);
}

static void
valent_mpris_impl_export_full_cb (ValentMPRISImpl     *impl,
                                  GAsyncResult        *result,
                                  MPRISAdapterFixture *fixture)
{
  GError *error = NULL;

  valent_mpris_impl_export_finish (impl, result, &error);
  g_assert_no_error (error);
}

static void
test_mpris_adapter_self (MPRISAdapterFixture *fixture,
                         gconstpointer        user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;

  g_signal_connect (fixture->media,
                    "items-changed",
                    G_CALLBACK (on_players_changed),
                    fixture);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  impl = valent_mpris_impl_new (fixture->export);

  VALENT_TEST_CHECK ("Adapter adds players when exported on the bus");
  valent_mpris_impl_export_full (impl,
                                 "org.mpris.MediaPlayer2.Test",
                                 NULL,
                                 (GAsyncReadyCallback)valent_mpris_impl_export_full_cb,
                                 NULL);
  valent_test_await_signal (fixture->media, "items-changed");
  g_assert_true (VALENT_IS_MEDIA_PLAYER (fixture->player));

  VALENT_TEST_CHECK ("Adapter removes players when unexported from the bus");
  valent_mpris_impl_unexport (impl);
  valent_test_await_signal (fixture->media, "items-changed");
  g_assert_null (fixture->player);

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
}

static void
test_mpris_adapter_player (MPRISAdapterFixture *fixture,
                           gconstpointer        user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;
  ValentMediaActions flags;
  ValentMediaRepeat repeat;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  double position;
  gboolean shuffle;

  g_signal_connect (fixture->media,
                    "items-changed",
                    G_CALLBACK (on_players_changed),
                    fixture);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  impl = valent_mpris_impl_new (fixture->export);

  VALENT_TEST_CHECK ("Adapter adds players when exported on the bus");
  valent_mpris_impl_export_full (impl,
                                 "org.mpris.MediaPlayer2.Test",
                                 NULL,
                                 (GAsyncReadyCallback)valent_mpris_impl_export_full_cb,
                                 NULL);
  valent_test_await_signal (fixture->media, "items-changed");
  g_assert_true (VALENT_IS_MEDIA_PLAYER (fixture->player));

  VALENT_TEST_CHECK ("GObject properties function correctly");
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
  g_assert_cmpfloat (position, <=, 0.0);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat (volume, >=, 1.0);
  g_clear_pointer (&name, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  /* Setters */
  VALENT_TEST_CHECK ("Player `set_position()` method works correctly");
  g_object_set (fixture->player, "position", 5.0, NULL);
  valent_test_await_signal (fixture->export, "notify::position");
  /* g_assert_cmpfloat_with_epsilon (valent_media_player_get_position (fixture->export), 5.0, 0.1); */

  VALENT_TEST_CHECK ("Player `set_repeat()` method works correctly");
  g_object_set (fixture->player, "repeat", VALENT_MEDIA_REPEAT_ALL, NULL);
  valent_test_await_signal (fixture->export, "notify::repeat");
  g_assert_cmpuint (valent_media_player_get_repeat (fixture->export), ==, VALENT_MEDIA_REPEAT_ALL);

  VALENT_TEST_CHECK ("Player `set_shuffle()` method works correctly");
  g_object_set (fixture->player, "shuffle", TRUE, NULL);
  valent_test_await_signal (fixture->export, "notify::shuffle");
  g_assert_true (valent_media_player_get_shuffle (fixture->export));

  VALENT_TEST_CHECK ("Player `set_volume()` method works correctly");
  g_object_set (fixture->player, "volume", 0.5, NULL);
  valent_test_await_signal (fixture->export, "notify::volume");
  g_assert_cmpfloat_with_epsilon (valent_media_player_get_volume (fixture->export), 0.5, 0.01);

  /* Methods */
  VALENT_TEST_CHECK ("Player `play()` method works correctly");
  valent_media_player_play (fixture->player);
  valent_test_await_signal (fixture->export, "notify::state");

  VALENT_TEST_CHECK ("Player `pause()` method works correctly");
  valent_media_player_pause (fixture->player);
  valent_test_await_signal (fixture->export, "notify::state");

  VALENT_TEST_CHECK ("Player `stop()` method works correctly");
  valent_media_player_stop (fixture->player);
  valent_test_await_signal (fixture->export, "notify::state");

  VALENT_TEST_CHECK ("Player `next()` method works correctly");
  valent_media_player_next (fixture->player);
  valent_test_await_signal (fixture->export, "notify::metadata");

  VALENT_TEST_CHECK ("Player `previous()` method works correctly");
  valent_media_player_previous (fixture->player);
  valent_test_await_signal (fixture->export, "notify::metadata");

  VALENT_TEST_CHECK ("Player `seek()` method works correctly");
  valent_media_player_seek (fixture->player, 1000);
  valent_test_await_signal (fixture->export, "notify::position");

  VALENT_TEST_CHECK ("Adapter removes players when unexported from the bus");
  valent_mpris_impl_unexport (impl);
  valent_test_await_signal (fixture->media, "items-changed");
  g_assert_null (fixture->player);

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
}

static void
test_mpris_adapter_export (MPRISAdapterFixture *fixture,
                           gconstpointer        user_data)
{
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;
  g_autoptr (ValentMediaPlayer) player_out = NULL;
  ValentMediaActions flags;
  ValentMediaRepeat repeat;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  double position;
  gboolean shuffle;

  g_signal_connect (fixture->media,
                    "items-changed",
                    G_CALLBACK (on_players_changed),
                    fixture);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  impl = valent_mpris_impl_new (fixture->export);

  VALENT_TEST_CHECK ("GObject properties function correctly (impl)");
  g_object_get (impl,
                "player", &player_out,
                NULL);
  g_assert_true (fixture->export == player_out);

  VALENT_TEST_CHECK ("Adapter exports objects on the bus");
  valent_mpris_impl_export_full (impl,
                                 "org.mpris.MediaPlayer2.Test",
                                 NULL,
                                 (GAsyncReadyCallback)valent_mpris_impl_export_full_cb,
                                 NULL);
  valent_test_await_signal (fixture->media, "items-changed");
  g_assert_true (VALENT_IS_MEDIA_PLAYER (fixture->player));

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->export,
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
  g_assert_cmpfloat (position, <=, 0.0);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat (volume, >=, 1.0);
  g_clear_pointer (&name, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  /* Setters */
  VALENT_TEST_CHECK ("Player `set_position()` method works correctly");
  g_object_set (fixture->export, "position", 5.0, NULL);
  valent_test_await_signal (fixture->player, "notify::position");
  /* g_assert_cmpfloat_with_epsilon (valent_media_player_get_position (fixture->player), 5.0, 0.1); */

  VALENT_TEST_CHECK ("Player `set_repeat()` method works correctly");
  g_object_set (fixture->export, "repeat", VALENT_MEDIA_REPEAT_ALL, NULL);
  valent_test_await_signal (fixture->player, "notify::repeat");
  g_assert_cmpuint (valent_media_player_get_repeat (fixture->player), ==, VALENT_MEDIA_REPEAT_ALL);

  VALENT_TEST_CHECK ("Player `set_shuffle()` method works correctly");
  g_object_set (fixture->export, "shuffle", TRUE, NULL);
  valent_test_await_signal (fixture->player, "notify::shuffle");
  g_assert_true (valent_media_player_get_shuffle (fixture->player));

  VALENT_TEST_CHECK ("Player `set_volume()` method works correctly");
  g_object_set (fixture->export, "volume", 0.5, NULL);
  valent_test_await_signal (fixture->player, "notify::volume");
  g_assert_cmpfloat_with_epsilon (valent_media_player_get_volume (fixture->player), 0.5, 0.01);

  /* Methods */
  VALENT_TEST_CHECK ("Player `play()` method works correctly");
  valent_media_player_play (fixture->export);
  valent_test_await_signal (fixture->player, "notify::state");

  VALENT_TEST_CHECK ("Player `pause()` method works correctly");
  valent_media_player_pause (fixture->export);
  valent_test_await_signal (fixture->player, "notify::state");

  VALENT_TEST_CHECK ("Player `stop()` method works correctly");
  valent_media_player_stop (fixture->export);
  valent_test_await_signal (fixture->player, "notify::state");

  VALENT_TEST_CHECK ("Player `next()` method works correctly");
  valent_media_player_next (fixture->export);
  valent_test_await_signal (fixture->player, "notify::metadata");

  VALENT_TEST_CHECK ("Player `previous()` method works correctly");
  valent_media_player_previous (fixture->export);
  valent_test_await_signal (fixture->player, "notify::metadata");

  VALENT_TEST_CHECK ("Player `seek()` method works correctly");
  valent_media_player_seek (fixture->export, 1000);
  valent_test_await_signal (fixture->player, "notify::position");

  VALENT_TEST_CHECK ("Adapter removes players when unexported from the bus");
  valent_mpris_impl_unexport (impl);
  valent_test_await_signal (fixture->media, "items-changed");
  g_assert_null (fixture->player);

  g_signal_handlers_disconnect_by_data (fixture->media, fixture);
}


int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mpris/adapter",
              MPRISAdapterFixture, NULL,
              mpris_adapter_fixture_set_up,
              test_mpris_adapter_self,
              mpris_adapter_fixture_tear_down);

  g_test_add ("/plugins/mpris/player",
              MPRISAdapterFixture, NULL,
              mpris_adapter_fixture_set_up,
              test_mpris_adapter_player,
              mpris_adapter_fixture_tear_down);

  g_test_add ("/plugins/mpris/export",
              MPRISAdapterFixture, NULL,
              mpris_adapter_fixture_set_up,
              test_mpris_adapter_export,
              mpris_adapter_fixture_tear_down);

  return g_test_run ();
}
