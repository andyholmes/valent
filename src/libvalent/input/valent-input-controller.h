// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INPUT_INSIDE) && !defined (VALENT_INPUT_COMPILATION)
# error "Only <libvalent-input.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-input-keydef.h"


G_BEGIN_DECLS

#define VALENT_TYPE_INPUT_CONTROLLER (valent_input_controller_get_type())

G_DECLARE_INTERFACE (ValentInputController, valent_input_controller, VALENT, INPUT_CONTROLLER, GObject)

struct _ValentInputControllerInterface
{
  GTypeInterface   g_iface;

  void             (*keyboard_keysym)  (ValentInputController *controller,
                                        unsigned int           keysym,
                                        gboolean               state);
  void             (*pointer_axis)     (ValentInputController *controller,
                                        double                 dx,
                                        double                 dy);
  void             (*pointer_button)   (ValentInputController *controller,
                                        ValentPointerButton    button,
                                        gboolean               state);
  void             (*pointer_motion)   (ValentInputController *controller,
                                        double                 dx,
                                        double                 dy);
  void             (*pointer_position) (ValentInputController *controller,
                                        double                 x,
                                        double                 y);
};

/* Core Interface */
void valent_input_controller_keyboard_keysym  (ValentInputController *controller,
                                               unsigned int           keysym,
                                               gboolean               state);
void valent_input_controller_pointer_axis     (ValentInputController *controller,
                                               double                 dx,
                                               double                 dy);
void valent_input_controller_pointer_button   (ValentInputController *controller,
                                               ValentPointerButton    button,
                                               gboolean               state);
void valent_input_controller_pointer_motion   (ValentInputController *controller,
                                               double                 dx,
                                               double                 dy);
void valent_input_controller_pointer_position (ValentInputController *controller,
                                               double                 x,
                                               double                 y);

G_END_DECLS

