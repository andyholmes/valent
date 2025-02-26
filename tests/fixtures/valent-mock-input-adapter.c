// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-input-adapter"

#include "config.h"

#include <glib-object.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-input-adapter.h"


struct _ValentMockInputAdapter
{
  ValentInputAdapter  parent_instance;
};

G_DEFINE_FINAL_TYPE (ValentMockInputAdapter, valent_mock_input_adapter, VALENT_TYPE_INPUT_ADAPTER)

/*
 * ValentInputAdapter
 */
static void
valent_mock_input_adapter_keyboard_keysym (ValentInputAdapter *adapter,
                                           uint32_t            keysym,
                                           gboolean            state)
{
  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));

  valent_test_event_push (g_strdup_printf ("KEYSYM %u %i", keysym, state));
}

static void
valent_mock_input_adapter_pointer_axis (ValentInputAdapter *adapter,
                                        double              dx,
                                        double              dy)
{
  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));
  g_assert (!G_APPROX_VALUE (dx, 0.0, 0.01) || !G_APPROX_VALUE (dy, 0.0, 0.01));

  valent_test_event_push (g_strdup_printf ("POINTER AXIS %.1f %.1f", dx, dy));
}

static void
valent_mock_input_adapter_pointer_button (ValentInputAdapter *adapter,
                                          unsigned int        button,
                                          gboolean            pressed)
{
  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));

  valent_test_event_push (g_strdup_printf ("POINTER BUTTON %u %i", button, pressed));
}

static void
valent_mock_input_adapter_pointer_motion (ValentInputAdapter *adapter,
                                          double              dx,
                                          double              dy)
{
  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));
  g_assert (!G_APPROX_VALUE (dx, 0.0, 0.01) || !G_APPROX_VALUE (dy, 0.0, 0.01));

  valent_test_event_push (g_strdup_printf ("POINTER MOTION %.1f %.1f", dx, dy));
}


/*
 * GObject
 */
static void
valent_mock_input_adapter_class_init (ValentMockInputAdapterClass *klass)
{
  ValentInputAdapterClass *adapter_class = VALENT_INPUT_ADAPTER_CLASS (klass);

  adapter_class->keyboard_keysym = valent_mock_input_adapter_keyboard_keysym;
  adapter_class->pointer_axis = valent_mock_input_adapter_pointer_axis;
  adapter_class->pointer_button = valent_mock_input_adapter_pointer_button;
  adapter_class->pointer_motion = valent_mock_input_adapter_pointer_motion;
}

static void
valent_mock_input_adapter_init (ValentMockInputAdapter *self)
{
}

