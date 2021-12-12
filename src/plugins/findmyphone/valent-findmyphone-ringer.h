// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <glib.h>

G_BEGIN_DECLS

typedef struct _ValentFindmyphoneRinger ValentFindmyphoneRinger;


ValentFindmyphoneRinger * valent_findmyphone_ringer_new      (void);
void                      valent_findmyphone_ringer_start    (ValentFindmyphoneRinger *ringer);
void                      valent_findmyphone_ringer_stop     (ValentFindmyphoneRinger *ringer);
void                      valent_findmyphone_ringer_show     (ValentFindmyphoneRinger *ringer);
void                      valent_findmyphone_ringer_hide     (ValentFindmyphoneRinger *ringer);

ValentFindmyphoneRinger * valent_findmyphone_ringer_acquire  (void);
void                      valent_findmyphone_ringer_release  (gpointer                 ringer);
void                      valent_findmyphone_ringer_toggle   (ValentFindmyphoneRinger *ringer,
                                                              gpointer                 owner);
gboolean                  valent_findmyphone_ringer_is_owner (ValentFindmyphoneRinger *ringer,
                                                              gpointer                 owner);

G_END_DECLS

