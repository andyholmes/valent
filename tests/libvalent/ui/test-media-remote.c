// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-media-player.h"

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentMediaRemote"))


static void
test_media_remote (void)
{
  GtkWindow *remote = NULL;
  GListStore *list = NULL;
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (GListStore) players = NULL;

  list = g_list_store_new (VALENT_TYPE_MEDIA_PLAYER);
  remote = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "players", list,
                         NULL);

  /* Properties */
  g_object_get (remote,
                "players", &players,
                NULL);
  g_assert_true (list == players);
  g_clear_object (&players);

  /* Show the window */
  gtk_window_present (remote);
  valent_test_await_pending ();

  /* Add a player */
  player = g_object_new (VALENT_TYPE_MOCK_MEDIA_PLAYER, NULL);
  valent_mock_media_player_update_flags (VALENT_MOCK_MEDIA_PLAYER (player),
                                         VALENT_MEDIA_ACTION_PLAY);
  g_list_store_append (list, player);

  /* Run through the available actions */
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.play", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.pause", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.play-pause", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.seek", "d", 1.0);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.seek", "d", -1.0);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.next", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.previous", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.stop", NULL);

  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.repeat", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.repeat", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.repeat", NULL);

  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.shuffle", NULL);
  gtk_widget_activate_action (GTK_WIDGET (remote), "remote.shuffle", NULL);

  /* Remove the player */
  g_list_store_remove (list, 0);

  /* Destroy the window */
  gtk_window_destroy (remote);
  valent_test_await_pending ();
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/ui/media-remote",
                   test_media_remote);

  return g_test_run ();
}

