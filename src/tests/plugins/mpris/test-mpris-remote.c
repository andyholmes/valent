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
  ValentMedia       *media;
  ValentMediaPlayer *player;
  gpointer           data;
  unsigned int       state;
} MprisRemoteFixture;


static void
mpris_remote_fixture_set_up (MprisRemoteFixture *fixture,
                               gconstpointer          user_data)
{
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
mpris_remote_fixture_tear_down (MprisRemoteFixture *fixture,
                                  gconstpointer          user_data)
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
  MprisRemoteFixture *fixture = user_data;
  const char *name, *old_owner, *new_owner;

  g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

  if (g_strcmp0 (name, "org.mpris.MediaPlayer2.Valent") == 0)
    g_main_loop_quit (fixture->loop);
}

static void
get_all_cb (GDBusConnection       *connection,
            GAsyncResult          *result,
            MprisRemoteFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
set_cb (GDBusConnection       *connection,
            GAsyncResult          *result,
            MprisRemoteFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
on_remote_method (ValentMprisRemote     *remote,
                  const char            *method,
                  GVariant              *args,
                  MprisRemoteFixture *fixture)
{
  test_mpris_remote_method (remote, method, args, fixture);

  fixture->state = TRUE;
  g_main_loop_quit (fixture->loop);
}

static void
on_remote_set_property (ValentMprisRemote  *remote,
                        const char         *name,
                        GVariant           *value,
                        MprisRemoteFixture *fixture)
{
  fixture->state = TRUE;
  g_main_loop_quit (fixture->loop);
}

typedef struct {
  const char *name;
  const char *value;
} DBusTest;

static void
test_mpris_remote_dbus (MprisRemoteFixture *fixture,
                         gconstpointer       user_data)
{
  g_autoptr (ValentMprisRemote) remote = NULL;
  g_autoptr (GDBusConnection) connection = NULL;
  g_autoptr (GFile) file = NULL;
  unsigned int watch_id;

  /* Create a new remote */
  remote = valent_mpris_remote_new ();
  valent_mpris_remote_set_name (remote, "Test Player");

  g_signal_connect (remote,
                    "method-call",
                    G_CALLBACK (on_remote_method),
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

  /* Export the remote */
  valent_mpris_remote_export (remote);
  g_main_loop_run (fixture->loop);

  /* Methods */

  static const char *action_methods[] = {
    "Play", "Pause", "PlayPause", "Next", "Previous", "Stop"
  };

  static const DBusTest player_properties[] = {
    {"LoopStatus", "'Track'"},
    {"LoopStatus", "'Playlist'"},
    {"Shuffle", "true"},
    {"Volume", "0.5"},
  };

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
                          g_variant_new ("(x)", 1000),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          NULL,
                          NULL);
  g_main_loop_run (fixture->loop);

  /* DBus Properties */
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
  file = g_file_new_for_path (TEST_DATA_DIR"/image.png");
  valent_mpris_remote_update_art (remote, file);

  /* Unexport the remote */
  valent_mpris_remote_unexport (remote);
  g_main_loop_run (fixture->loop);

  g_dbus_connection_signal_unsubscribe (connection, watch_id);
}

static void
test_mpris_remote_player (MprisRemoteFixture *fixture,
                          gconstpointer       user_data)
{
  ValentMediaPlayer *player;
  g_autoptr (ValentMprisRemote) remote = NULL;
  g_autoptr (GDBusConnection) connection = NULL;
  unsigned int watch_id;

  /* org.mpris.MediaPlayer2.Player */
  ValentMediaActions flags;
  ValentMediaState state;
  double volume;
  char *name;
  g_autoptr (GVariant) metadata = NULL;
  gint64 position;

  /* Create a new remote */
  remote = valent_mpris_remote_new ();
  player = VALENT_MEDIA_PLAYER (remote);
  valent_mpris_remote_set_name (remote, "Test Player");

  g_signal_connect (remote,
                    "method-call",
                    G_CALLBACK (on_remote_method),
                    fixture);

  g_signal_connect (remote,
                    "set-property",
                    G_CALLBACK (on_remote_set_property),
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

  /* Export the remote */
  valent_mpris_remote_export (remote);
  g_main_loop_run (fixture->loop);

  /* Test Player Properties */
  g_object_get (remote,
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

  g_object_set (remote,
                "state",  (VALENT_MEDIA_STATE_REPEAT_ALL |
                           VALENT_MEDIA_STATE_SHUFFLE),
                "volume", 1.0,
                NULL);

  /* Test Player Methods */
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

  valent_media_player_open_uri (player, "https://andyholmes.ca");
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  valent_media_player_seek (player, 1000);
  g_assert_true (fixture->state);
  fixture->state = FALSE;

  //valent_media_player_set_position (player, "/dbus/path", 5);
  //g_assert_cmpint (valent_media_player_get_position (player), ==, 5);

  /* Remove Player */
  valent_mpris_remote_unexport (remote);
  //g_main_loop_run (fixture->loop);

  g_dbus_connection_signal_unsubscribe (connection, watch_id);
  g_signal_handlers_disconnect_by_data (remote, fixture);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/plugins/mpris/remote/dbus",
              MprisRemoteFixture, NULL,
              mpris_remote_fixture_set_up,
              test_mpris_remote_dbus,
              mpris_remote_fixture_tear_down);

  g_test_add ("/plugins/mpris/remote/player",
              MprisRemoteFixture, NULL,
              mpris_remote_fixture_set_up,
              test_mpris_remote_player,
              mpris_remote_fixture_tear_down);

  return g_test_run ();
}
