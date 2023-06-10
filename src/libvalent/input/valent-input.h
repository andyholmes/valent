// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-component.h"
#include "valent-input-adapter.h"

G_BEGIN_DECLS

#define VALENT_TYPE_INPUT (valent_input_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentInput, valent_input, VALENT, INPUT, ValentComponent)

VALENT_AVAILABLE_IN_1_0
ValentInput * valent_input_get_default      (void);
VALENT_AVAILABLE_IN_1_0
void          valent_input_export_adapter   (ValentInput        *input,
                                             ValentInputAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
void          valent_input_unexport_adapter (ValentInput        *input,
                                             ValentInputAdapter *adapter);
VALENT_AVAILABLE_IN_1_0
void          valent_input_keyboard_keysym  (ValentInput        *input,
                                             uint32_t            keysym,
                                             gboolean            state);
VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_axis     (ValentInput        *input,
                                             double              dx,
                                             double              dy);
VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_button   (ValentInput        *input,
                                             unsigned int        button,
                                             gboolean            state);
VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_motion   (ValentInput        *input,
                                             double              dx,
                                             double              dy);

G_END_DECLS

