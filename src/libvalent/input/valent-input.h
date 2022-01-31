// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INPUT_INSIDE) && !defined (VALENT_INPUT_COMPILATION)
# error "Only <libvalent-input.h> can be included directly."
#endif

#include <glib-object.h>
#include <gdk/gdk.h>
#include <libvalent-core.h>

#include "valent-input-keydef.h"

G_BEGIN_DECLS

#define VALENT_TYPE_INPUT (valent_input_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentInput, valent_input, VALENT, INPUT, ValentComponent)

VALENT_AVAILABLE_IN_1_0
ValentInput * valent_input_get_default      (void);

VALENT_AVAILABLE_IN_1_0
void          valent_input_keyboard_action  (ValentInput         *input,
                                             unsigned int         keysym,
                                             GdkModifierType      mask);
VALENT_AVAILABLE_IN_1_0
void          valent_input_keyboard_keysym  (ValentInput         *input,
                                             unsigned int         keysym,
                                             gboolean             state);
VALENT_AVAILABLE_IN_1_0
void          valent_input_keyboard_mask    (ValentInput         *input,
                                             GdkModifierType      mask,
                                             gboolean             lock);

VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_axis     (ValentInput         *input,
                                             double               dx,
                                             double               dy);
VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_button   (ValentInput         *input,
                                             ValentPointerButton  button,
                                             gboolean             state);
VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_click    (ValentInput         *input,
                                             ValentPointerButton  button);
VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_motion   (ValentInput         *input,
                                             double               dx,
                                             double               dy);
VALENT_AVAILABLE_IN_1_0
void          valent_input_pointer_position (ValentInput         *input,
                                             double               x,
                                             double               y);

G_END_DECLS

