// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_SESSION_INSIDE) && !defined (VALENT_SESSION_COMPILATION)
# error "Only <libvalent-session.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SESSION_ADAPTER (valent_session_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentSessionAdapter, valent_session_adapter, VALENT, SESSION_ADAPTER, GObject)

struct _ValentSessionAdapterClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  gboolean       (*get_active) (ValentSessionAdapter *adapter);
  gboolean       (*get_locked) (ValentSessionAdapter *adapter);
  void           (*set_locked) (ValentSessionAdapter *adapter,
                                gboolean              state);

  /* signals */
  void           (*changed)    (ValentSessionAdapter *adapter);
};

VALENT_AVAILABLE_IN_1_0
void       valent_session_adapter_emit_changed (ValentSessionAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
gboolean   valent_session_adapter_get_active   (ValentSessionAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
gboolean   valent_session_adapter_get_locked   (ValentSessionAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
void       valent_session_adapter_set_locked   (ValentSessionAdapter *adapter,
                                                gboolean              state);

G_END_DECLS

