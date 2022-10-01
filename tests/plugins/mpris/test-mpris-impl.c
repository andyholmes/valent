// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-media.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"
#include "valent-mpris-adapter.h"
#include "valent-mpris-impl.h"
#include "valent-mpris-player.h"


typedef struct
{
  GMainLoop         *loop;
  ValentMedia       *media;
  ValentMediaPlayer *player;
  gpointer           data;
  unsigned int       state;
} MPRISImplFixture;


static void
mpris_impl_fixture_set_up (MPRISImplFixture *fixture,
                           gconstpointer     user_data)
{
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
mpris_impl_fixture_tear_down (MPRISImplFixture *fixture,
                              gconstpointer     user_data)
{
  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
on_name_owner_changed (GDBusConnection *connection,
                       const char      *sender_name,
                       const char      *object_path,
                       const char      *interface_name,
                       const char      *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  MPRISImplFixture *fixture = user_data;
  const char *name, *old_owner, *new_owner;

  g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

  if (g_strcmp0 (name, "org.mpris.MediaPlayer2.Valent") == 0)
    g_main_loop_quit (fixture->loop);
}

static void
get_all_cb (GDBusConnection  *connection,
            GAsyncResult     *result,
            MPRISImplFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
set_cb (GDBusConnection  *connection,
        GAsyncResult     *result,
        MPRISImplFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
on_dbus_notify (ValentMediaPlayer *player,
                GParamSpec        *pspec,
                MPRISImplFixture  *fixture)
{
  fixture->state = TRUE;
  g_main_loop_quit (fixture->loop);
}

static void
on_player_notify (ValentMediaPlayer *player,
                  GParamSpec        *pspec,
                  MPRISImplFixture  *fixture)
{
  fixture->state = TRUE;
}

typedef struct {
  const char *name;
  const char *value;
} DBusTest;

static void
test_mpris_impl_dbus (MPRISImplFixture *fixture,
                      gconstpointer     user_data)
{
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;
  g_autoptr (GDBusConnection) connection = NULL;
  /* g_autoptr (GFile) file = NULL; */
  unsigned int watch_id;

  /* Watch for the exported service */
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  watch_id = g_dbus_connection_signal_subscribe (connection,
                                                 "org.freedesktop.DBus",
                                                 "org.freedesktop.DBus",
                                                 "NameOwnerChanged",
                                                 "/org/freedesktop/DBus",
                                                 "org.mpris.MediaPlayer2",
                                                 G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
                                                 on_name_owner_changed,
                                                 fixture, NULL);

  /* Export the impl */
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  impl = valent_mpris_impl_new (player);
  valent_mpris_impl_export (impl, connection, NULL);
  g_main_loop_run (fixture->loop);

  /* Player Methods */
  static const char *action_methods[] = {
    "Play", "Pause", "PlayPause", "Next", "Previous", "Stop"
  };

  g_signal_connect (player,
                    "notify",
                    G_CALLBACK (on_dbus_notify),
                    fixture);

  for (unsigned int i = 0; i < G_N_ELEMENTS (action_methods); i++)
    {
      g_dbus_connection_call (connection,
                              "org.mpris.MediaPlayer2.Valent",
                              "/org/mpris/MediaPlayer2",
                              "org.mpris.MediaPlayer2.Player",
                              action_methods[i],
                              NULL,
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL,
                              NULL,
                              NULL);
      g_main_loop_run (fixture->loop);
    }

  g_dbus_connection_call (connection,
                          "org.mpris.MediaPlayer2.Valent",
                          "/org/mpris/MediaPlayer2",
                          "org.mpris.MediaPlayer2.Player",
                          "Seek",
                          g_variant_new ("(x)", 1000000),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
  g_main_loop_run (fixture->loop);

  g_signal_handlers_disconnect_by_data (player, fixture);

  /* Player Properties */
  static const DBusTest player_properties[] = {
    {"LoopStatus", "'Track'"},
    {"LoopStatus", "'Playlist'"},
    {"Shuffle", "true"},
    {"Volume", "0.5"},
  };

  g_dbus_connection_call (connection,
                          "org.mpris.MediaPlayer2.Valent",
                          "/org/mpris/MediaPlayer2",
                          "org.freedesktop.DBus.Properties",
                          "GetAll",
                          g_variant_new ("(s)", "org.mpris.MediaPlayer2"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)get_all_cb,
                          fixture);
  g_main_loop_run (fixture->loop);

  g_dbus_connection_call (connection,
                          "org.mpris.MediaPlayer2.Valent",
                          "/org/mpris/MediaPlayer2",
                          "org.freedesktop.DBus.Properties",
                          "Set",
                          g_variant_new ("(ssv)",
                                         "org.mpris.MediaPlayer2",
                                         "Fullscreen",
                                         g_variant_new_boolean (TRUE)),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)set_cb,
                          fixture);
  g_main_loop_run (fixture->loop);

  g_dbus_connection_call (connection,
                          "org.mpris.MediaPlayer2.Valent",
                          "/org/mpris/MediaPlayer2",
                          "org.freedesktop.DBus.Properties",
                          "GetAll",
                          g_variant_new ("(s)", "org.mpris.MediaPlayer2.Player"),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)get_all_cb,
                          fixture);
  g_main_loop_run (fixture->loop);

  for (unsigned int i = 0; i < G_N_ELEMENTS (player_properties); i++)
    {
      const DBusTest test = player_properties[i];

      g_dbus_connection_call (connection,
                              "org.mpris.MediaPlayer2.Valent",
                              "/org/mpris/MediaPlayer2",
                              "org.freedesktop.DBus.Properties",
                              "Set",
                              g_variant_new ("(ssv)",
                                             "org.mpris.MediaPlayer2.Player",
                                             test.name,
                                             g_variant_new_parsed (test.value)),
                              NULL,
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL,
                              (GAsyncReadyCallback)set_cb,
                              fixture);
      g_main_loop_run (fixture->loop);
    }

  /* Other */
  /* file = g_file_new_for_path (TEST_DATA_DIR"/image.png"); */
  /* valent_mpris_impl_update_art (impl, file); */

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Unexport the impl */
  valent_mpris_impl_unexport (impl);
  g_main_loop_run (fixture->loop);

  g_dbus_connection_signal_unsubscribe (connection, watch_id);
}

static void
test_mpris_impl_player (MPRISImplFixture *fixture,
                        gconstpointer     user_data)
{
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (ValentMPRISImpl) impl = NULL;
  g_autoptr (GDBusConnection) connection = NULL;
  unsigned int watch_id;

  /* org.mpris.MediaPlayer2.Player */
  ValentMediaActions flags;
  ValentMediaState repeat;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  gint64 position;
  gboolean shuffle;

  /* Create a new impl */
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  impl = valent_mpris_impl_new (player);

  g_signal_connect (player,
                    "notify",
                    G_CALLBACK (on_player_notify),
                    fixture);

  /* Watch for the exported service */
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  watch_id = g_dbus_connection_signal_subscribe (connection,
                                                 "org.freedesktop.DBus",
                                                 "org.freedesktop.DBus",
                                                 "NameOwnerChanged",
                                                 "/org/freedesktop/DBus",
                                                 "org.mpris.MediaPlayer2",
                                                 G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE,
                                                 on_name_owner_changed,
                                                 fixture, NULL);

  /* Export the impl */
  valent_mpris_impl_export (impl, connection, NULL);
  g_main_loop_run (fixture->loop);

  /* Mock Player Properties */
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
  g_assert_cmpint (position, ==, 0);
  g_assert_cmpuint (repeat, ==, VALENT_MEDIA_REPEAT_NONE);
  g_assert_false (shuffle);
  g_assert_cmpuint (state, ==, VALENT_MEDIA_STATE_STOPPED);
  g_assert_cmpfloat (volume, ==, 1.0);
  g_clear_pointer (&name, g_free);
  g_clear_pointer (&metadata, g_variant_unref);

  g_object_set (player,
                "shuffle", TRUE,
                "repeat",  VALENT_MEDIA_REPEAT_ALL,
                "volume",  1.0,
                NULL);

  /* Mock Player Methods */
  valent_media_player_play (player);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_play_pause (player);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_pause (player);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_stop (player);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_next (player);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_previous (player);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_seek (player, 1000);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_set_position (player, 2000);
  g_assert_cmpint (valent_media_player_get_position (player), ==, 2000);

  /* Remove Player */
  valent_mpris_impl_unexport (impl);
  //g_main_loop_run (fixture->loop);

  g_dbus_connection_signal_unsubscribe (connection, watch_id);
  g_signal_handlers_disconnect_by_data (impl, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mpris/impl/dbus",
              MPRISImplFixture, NULL,
              mpris_impl_fixture_set_up,
              test_mpris_impl_dbus,
              mpris_impl_fixture_tear_down);

  g_test_add ("/plugins/mpris/impl/player",
              MPRISImplFixture, NULL,
              mpris_impl_fixture_set_up,
              test_mpris_impl_player,
              mpris_impl_fixture_tear_down);

  return g_test_run ();
}

