// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

/**
 * VALENT_MPRIS_DBUS_NAME: (value "org.mpris.MediaPlayer2.Valent")
 *
 * The well-known name Valent exports its MPRIS player on.
 */
#define VALENT_MPRIS_DBUS_NAME "org.mpris.MediaPlayer2.Valent"

/**
 * VALENT_MPRIS_APPLICATION_INFO:
 *
 * A #GDBusInterfaceInfo describing the `org.mpris.MediaPlayer2` interface.
 */
#define VALENT_MPRIS_APPLICATION_INFO (valent_mpris_get_application_iface())

/**
 * VALENT_MPRIS_PLAYER_INFO:
 *
 * A #GDBusInterfaceInfo describing the `org.mpris.MediaPlayer2.Player`
 * interface.
 */
#define VALENT_MPRIS_PLAYER_INFO (valent_mpris_get_player_iface())


GDBusInterfaceInfo * valent_mpris_get_application_iface (void);
GDBusInterfaceInfo * valent_mpris_get_player_iface      (void);

ValentMediaRepeat    valent_mpris_repeat_from_string    (const char        *loop_status);
const char         * valent_mpris_repeat_to_string      (ValentMediaRepeat  repeat);
ValentMediaState     valent_mpris_state_from_string     (const char        *playback_status);
const char         * valent_mpris_state_to_string       (ValentMediaState   state);
double               valent_mpris_get_time              (void);

#define valent_mpris_play_pause(player) \
  G_STMT_START {                                                         \
      ValentMediaActions flags = valent_media_player_get_flags (player); \
      ValentMediaState state = valent_media_player_get_state (player);   \
                                                                         \
      if (state == VALENT_MEDIA_STATE_PLAYING &&                         \
          (flags & VALENT_MEDIA_ACTION_PAUSE) != 0)                      \
        valent_media_player_pause (VALENT_MEDIA_PLAYER (player));        \
                                                                         \
      else if (state != VALENT_MEDIA_STATE_PLAYING &&                    \
               (flags & VALENT_MEDIA_ACTION_PLAY) != 0)                  \
        valent_media_player_play (VALENT_MEDIA_PLAYER (player));         \
  } G_STMT_END                                                           \

G_END_DECLS
