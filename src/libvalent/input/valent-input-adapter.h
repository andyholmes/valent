// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INPUT_INSIDE) && !defined (VALENT_INPUT_COMPILATION)
# error "Only <libvalent-input.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_INPUT_ADAPTER (valent_input_adapter_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_DERIVABLE_TYPE (ValentInputAdapter, valent_input_adapter, VALENT, INPUT_ADAPTER, ValentObject)

struct _ValentInputAdapterClass
{
  ValentObjectClass   parent_class;

  /* virtual functions */
  void                (*keyboard_keysym) (ValentInputAdapter *adapter,
                                          unsigned int        keysym,
                                          gboolean            state);
  void                (*pointer_axis)    (ValentInputAdapter *adapter,
                                          double              dx,
                                          double              dy);
  void                (*pointer_button)  (ValentInputAdapter *adapter,
                                          unsigned int        button,
                                          gboolean            state);
  void                (*pointer_motion)  (ValentInputAdapter *adapter,
                                          double              dx,
                                          double              dy);

  /*< private >*/
  gpointer            padding[8];
};

VALENT_AVAILABLE_IN_1_0
void   valent_input_adapter_keyboard_keysym (ValentInputAdapter *adapter,
                                             unsigned int        keysym,
                                             gboolean            state);
VALENT_AVAILABLE_IN_1_0
void   valent_input_adapter_pointer_axis    (ValentInputAdapter *adapter,
                                             double              dx,
                                             double              dy);
VALENT_AVAILABLE_IN_1_0
void   valent_input_adapter_pointer_button  (ValentInputAdapter *adapter,
                                             unsigned int        button,
                                             gboolean            state);
VALENT_AVAILABLE_IN_1_0
void   valent_input_adapter_pointer_motion  (ValentInputAdapter *adapter,
                                             double              dx,
                                             double              dy);

G_END_DECLS

