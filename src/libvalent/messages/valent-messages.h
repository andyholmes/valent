// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-component.h"
#include "valent-messages-adapter.h"

G_BEGIN_DECLS

/**
 * VALENT_MESSAGES_GRAPH:
 *
 * The SPARQL graph name for messages in Valent.
 *
 * Since: 1.0
 */
#define VALENT_MESSAGES_GRAPH "valent:messages"


#define VALENT_TYPE_MESSAGES (valent_messages_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentMessages, valent_messages, VALENT, MESSAGES, ValentComponent)

VALENT_AVAILABLE_IN_1_0
ValentMessages * valent_messages_get_default      (void);

VALENT_AVAILABLE_IN_1_0
void             valent_messages_export_adapter   (ValentMessages        *messages,
                                                   ValentMessagesAdapter *object);
VALENT_AVAILABLE_IN_1_0
void             valent_messages_unexport_adapter (ValentMessages        *messages,
                                                   ValentMessagesAdapter *object);

G_END_DECLS

