// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gtk/gtk.h>
#include <valent.h>
#include <libvalent-test.h>

#include "test-gnome-common.h"

#define VALENT_TYPE_TEST_SUBJECT (g_type_from_name ("ValentMediaWindow"))


static void
test_media_remote (void)
{
  GtkWindow *remote = NULL;
  ValentMediaAdapter *adapter;
  g_autoptr (ValentInput) mixers = NULL;
  g_autoptr (ValentMedia) players = NULL;
  g_autoptr (ValentMediaPlayer) player = NULL;

  VALENT_TEST_CHECK ("Window can be constructed");
  remote = g_object_new (VALENT_TYPE_TEST_SUBJECT,
                         "mixers",  valent_input_get_default (),
                         "players", valent_media_get_default (),
                         NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (remote,
                "mixers",  &mixers,
                "players", &players,
                NULL);
  g_assert_true (VALENT_IS_INPUT (mixers));
  g_assert_true (VALENT_IS_MEDIA (players));

  VALENT_TEST_CHECK ("Window can be presented");
  gtk_window_present (remote);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("Window can add players");
  adapter = valent_test_await_adapter (valent_media_get_default ());
  g_action_group_activate_action (G_ACTION_GROUP (adapter), "add-player", NULL);

  VALENT_TEST_CHECK ("Window can add players");
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

  VALENT_TEST_CHECK ("Window can remove players");
  player = g_list_model_get_item (G_LIST_MODEL (adapter), 0);
  valent_media_adapter_player_removed (adapter, player);

  VALENT_TEST_CHECK ("Window can be destroyed");
  gtk_window_destroy (remote);
  valent_test_await_pending ();
}

int
main (int   argc,
      char *argv[])
{
  valent_test_gnome_init (&argc, &argv, NULL);

  g_test_add_func ("/plugins/gnome/media-remote",
                   test_media_remote);

  return g_test_run ();
}

