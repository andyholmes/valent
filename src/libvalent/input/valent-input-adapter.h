// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INPUT_INSIDE) && !defined (VALENT_INPUT_COMPILATION)
# error "Only <libvalent-input.h> can be included directly."
#endif

#include <glib-object.h>

#include "valent-input-keydef.h"

G_BEGIN_DECLS

#define VALENT_TYPE_INPUT_ADAPTER (valent_input_adapter_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentInputAdapter, valent_input_adapter, VALENT, INPUT_ADAPTER, GObject)

struct _ValentInputAdapterClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*keyboard_keysym)  (ValentInputAdapter *adapter,
                                      unsigned int        keysym,
                                      gboolean            state);
  void           (*pointer_axis)     (ValentInputAdapter *adapter,
                                      double              dx,
                                      double              dy);
  void           (*pointer_button)   (ValentInputAdapter *adapter,
                                      ValentPointerButton button,
                                      gboolean            state);
  void           (*pointer_motion)   (ValentInputAdapter *adapter,
                                      double              dx,
                                      double              dy);
  void           (*pointer_position) (ValentInputAdapter *adapter,
                                      double              x,
                                      double              y);
};

void   valent_input_adapter_keyboard_keysym  (ValentInputAdapter  *adapter,
                                              unsigned int         keysym,
                                              gboolean             state);
void   valent_input_adapter_pointer_axis     (ValentInputAdapter  *adapter,
                                              double               dx,
                                              double               dy);
void   valent_input_adapter_pointer_button   (ValentInputAdapter  *adapter,
                                              ValentPointerButton  button,
                                              gboolean             state);
void   valent_input_adapter_pointer_motion   (ValentInputAdapter  *adapter,
                                              double               dx,
                                              double               dy);
void   valent_input_adapter_pointer_position (ValentInputAdapter  *adapter,
                                              double               x,
                                              double               y);

G_END_DECLS

