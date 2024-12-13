// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentMediaWindow"))


static void
test_media_remote (void)
{
  GtkWindow *remote = NULL;
  ValentMediaAdapter *adapter;
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (ValentMedia) players = NULL;

  remote = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "players", valent_media_get_default (),
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (remote,
                "players", &players,
                NULL);
  g_assert_true (valent_media_get_default () == players);
  g_clear_object (&players);

  /* Show the window */
  gtk_window_present (remote);
  valent_test_await_pending ();

  /* Add a player */
  adapter = valent_test_await_adapter (valent_media_get_default ());
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  valent_mock_media_player_update_flags (VALENT_MOCK_MEDIA_PLAYER (player),
                                         VALENT_MEDIA_ACTION_PLAY);
  valent_media_adapter_player_added (adapter, player);

  /* Run through the available actions */
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.play", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.pause", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.play-pause", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.next", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.previous", NULL);

  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.repeat", "s", "none");
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.repeat", "s", "one");
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.repeat", "s", "all");

  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.shuffle", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.shuffle", NULL);

  /* Remove the player */
  valent_media_adapter_player_removed (adapter, player);

  /* Destroy the window */
  gtk_window_destroy (remote);
  valent_test_await_pending ();
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/media-remote",
                   test_media_remote);

  return g_test_run ();
}

