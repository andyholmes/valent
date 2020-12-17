// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_SESSION_INSIDE) && !defined (VALENT_SESSION_COMPILATION)
# error "Only <libvalent-session.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SESSION (valent_session_get_type ())

G_DECLARE_FINAL_TYPE (ValentSession, valent_session, VALENT, SESSION, ValentComponent)

gboolean        valent_session_get_active  (ValentSession *session);
gboolean        valent_session_get_locked  (ValentSession *session);
void            valent_session_set_locked  (ValentSession *session,
                                            gboolean       state);

ValentSession * valent_session_get_default (void);

G_END_DECLS

