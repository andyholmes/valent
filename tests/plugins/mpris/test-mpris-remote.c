// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-core.h>
#include <libvalent-test.h>
#include <libvalent-media.h>
#include <libvalent-ui.h>

#include "valent-mpris-remote.h"
#include "valent-mock-media-player.h"


static void
test_mpris_remote (ValentTestFixture *fixture,
                   gconstpointer      user_data)
{
  GtkWindow *remote = NULL;
  ValentDevice *device = NULL;
  GListStore *list = NULL;
  g_autoptr (ValentMediaPlayer) player = NULL;
  g_autoptr (GListStore) players = NULL;

  list = g_list_store_new (VALENT_TYPE_MEDIA_PLAYER);
  remote = g_object_new (VALENT_TYPE_MPRIS_REMOTE,
                         "device",  fixture->device,
                         "players", list,
                         NULL);


  /* Properties */
  g_object_get (remote,
                "device",  &device,
                "players", &players,
                NULL);
  g_assert_true (fixture->device == device);
  g_assert_true (list == players);
  g_clear_object (&device);
  g_clear_object (&players);

  /* Show the window */
  gtk_window_present (remote);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

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

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-mpris.json";

  valent_test_ui_init (&argc, &argv, NULL);

  g_test_add ("/plugins/mpris/remote",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_mpris_remote,
              valent_test_fixture_clear);

  return g_test_run ();
}

