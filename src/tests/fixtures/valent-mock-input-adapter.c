// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-input-adapter"

#include "config.h"

#include <glib-object.h>
#include <libvalent-core.h>
#include <libvalent-input.h>
#include <libvalent-test.h>

#include "valent-mock-input-adapter.h"


struct _ValentMockInputAdapter
{
  ValentInputAdapter  parent_instance;
};

G_DEFINE_TYPE (ValentMockInputAdapter, valent_mock_input_adapter, VALENT_TYPE_INPUT_ADAPTER)


static ValentInputAdapter *test_instance = NULL;

/*
 * ValentInputAdapter
 */
static void
valent_mock_input_adapter_keyboard_keysym (ValentInputAdapter *adapter,
                                           unsigned int        keysym,
                                           gboolean            state)
{
  char *event;

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));

  event = g_strdup_printf ("KEYSYM %u %i", keysym, state);
  valent_test_event_push (event);
}

static void
valent_mock_input_adapter_pointer_axis (ValentInputAdapter *adapter,
                                        double              dx,
                                        double              dy)
{
  char *event;

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));
  g_assert (dx != 0.0 || dy != 0.0);

  event = g_strdup_printf ("POINTER AXIS %.1f %.1f", dx, dy);
  valent_test_event_push (event);
}

static void
valent_mock_input_adapter_pointer_button (ValentInputAdapter *adapter,
                                          unsigned int        button,
                                          gboolean            pressed)
{
  char *event;

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));

  event = g_strdup_printf ("POINTER BUTTON %u %i", button, pressed);
  valent_test_event_push (event);
}

static void
valent_mock_input_adapter_pointer_motion (ValentInputAdapter *adapter,
                                          double              dx,
                                          double              dy)
{
  char *event;

  g_assert (VALENT_IS_INPUT_ADAPTER (adapter));
  g_assert (VALENT_IS_MOCK_INPUT_ADAPTER (adapter));
  g_assert (dx != 0 || dy != 0);

  event = g_strdup_printf ("POINTER MOTION %.1f %.1f", dx, dy);
  valent_test_event_push (event);
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
  if (test_instance == NULL)
    {
      test_instance = VALENT_INPUT_ADAPTER (self);
      g_object_add_weak_pointer (G_OBJECT (test_instance),
                                 (gpointer)&test_instance);
    }
}

/**
 * valent_mock_input_adapter_get_instance:
 *
 * Get the #ValentMockInputAdapter instance.
 *
 * Returns: (transfer none) (nullable): a #ValentInputAdapter
 */
ValentInputAdapter *
valent_mock_input_adapter_get_instance (void)
{
  return test_instance;
}

