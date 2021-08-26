// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <libvalent-media.h>

#include "valent-mpris-remote.h"


static inline void
test_mpris_remote_method (ValentMprisRemote *remote,
                          const char        *method,
                          GVariant          *args,
                          gpointer           user_data)
{
  GVariantDict dict;
  static const char * const artist[] = { "Test Artist" };
  ValentMediaActions flags = VALENT_MEDIA_ACTION_NONE;

  /* Fake playback start */
  if (g_strcmp0 (method, "Play") == 0 || g_strcmp0 (method, "Previous") == 0)
    {
      g_variant_dict_init (&dict, NULL);
      g_variant_dict_insert_value (&dict, "xesam:artist", g_variant_new_strv (artist, 1));
      g_variant_dict_insert (&dict, "xesam:title", "s", "Track 1");
      g_variant_dict_insert (&dict, "xesam:album", "s", "Test Album");
      g_variant_dict_insert (&dict, "mpris:length", "x", G_TIME_SPAN_MINUTE * 3);

      flags |= (VALENT_MEDIA_ACTION_NEXT |
                VALENT_MEDIA_ACTION_PAUSE |
                VALENT_MEDIA_ACTION_SEEK);

      valent_mpris_remote_update_player (remote,
                                         flags,
                                         g_variant_dict_end (&dict),
                                         "Playing",
                                         0,
                                         1.0);
    }

  /* Fake track next */
  else if (g_strcmp0 (method, "Next") == 0)
    {
      g_variant_dict_init (&dict, NULL);
      g_variant_dict_insert_value (&dict, "xesam:artist", g_variant_new_strv (artist, 1));
      g_variant_dict_insert (&dict, "xesam:title", "s", "Track 2");
      g_variant_dict_insert (&dict, "xesam:album", "s", "Test Album");
      g_variant_dict_insert (&dict, "mpris:length", "x", G_TIME_SPAN_MINUTE * 3);

      flags |= (VALENT_MEDIA_ACTION_PREVIOUS |
                VALENT_MEDIA_ACTION_PAUSE |
                VALENT_MEDIA_ACTION_SEEK);

      valent_mpris_remote_update_player (remote,
                                         flags,
                                         g_variant_dict_end (&dict),
                                         "Playing",
                                         0,
                                         1.0);
    }

  /* Fake playback pause */
  else if (g_strcmp0 (method, "Pause") == 0)
    {

      flags |= (VALENT_MEDIA_ACTION_NEXT |
                VALENT_MEDIA_ACTION_PREVIOUS |
                VALENT_MEDIA_ACTION_PLAY |
                VALENT_MEDIA_ACTION_SEEK);

      valent_mpris_remote_update_player (remote,
                                         flags,
                                         NULL,
                                         "Paused",
                                         0,
                                         1.0);
    }

  /* Fake seek */
  else if (g_strcmp0 (method, "Seek") == 0)
    {
      gint64 offset;

      g_variant_get (args, "(x)", &offset);
      valent_mpris_remote_emit_seeked (remote, offset);
    }

  /* Fake playback stop */
  else if (g_strcmp0 (method, "Stop") == 0)
    {
      g_variant_dict_init (&dict, NULL);

      valent_mpris_remote_update_player (remote,
                                         flags,
                                         g_variant_dict_end (&dict),
                                         "Stopped",
                                         0,
                                         1.0);
    }
}

static inline void
test_mpris_remote_set_property (ValentMprisRemote *remote,
                                const char        *name,
                                GVariant          *value,
                                gpointer           user_data)
{
  if (g_strcmp0 (name, "Volume") == 0)
    valent_mpris_remote_update_volume (remote, g_variant_get_double (value));
}

static void
test_mpris_remote_export (ValentMprisRemote *remote,
                          GAsyncResult      *result,
                          GMainLoop         *loop)
{
  GError *error = NULL;

  valent_mpris_remote_export_finish (remote, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (loop);
}

static ValentMprisRemote *
valent_test_mpris_get_remote (void)
{
  ValentMprisRemote *remote;
  g_autoptr (GMainLoop) loop = NULL;

  loop = g_main_loop_new (NULL, FALSE);

  /* Export a mock player that we can use during testing */
  remote = valent_mpris_remote_new ();
  valent_mpris_remote_set_name (remote, "Test Player");
  valent_mpris_remote_export_full (remote,
                                   "org.mpris.MediaPlayer2.Test",
                                   NULL,
                                   (GAsyncReadyCallback)test_mpris_remote_export,
                                   loop);
  g_main_loop_run (loop);

  g_signal_connect (remote,
                    "method-call",
                    G_CALLBACK (test_mpris_remote_method),
                    NULL);
  g_signal_connect (remote,
                    "set-property",
                    G_CALLBACK (test_mpris_remote_set_property),
                    NULL);

  return remote;
}

