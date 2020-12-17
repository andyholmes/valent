// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-xdp-controller"

#include "config.h"

#include <glib-object.h>
#include <libvalent-core.h>
#include <libvalent-input.h>
#include <libvalent-test.h>

#include "valent-test-input-controller.h"


struct _ValentTestInputController
{
  PeasExtensionBase  parent_instance;
};

/* Interfaces */
static void valent_input_controller_iface_init (ValentInputControllerInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentTestInputController, valent_test_input_controller, PEAS_TYPE_EXTENSION_BASE,
                         G_IMPLEMENT_INTERFACE (VALENT_TYPE_INPUT_CONTROLLER, valent_input_controller_iface_init))


/*
 * ValentInputController
 */
static void
valent_test_input_controller_keyboard_keysym (ValentInputController *controller,
                                              guint                  keysym,
                                              gboolean               state)
{
  char *event;

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_TEST_INPUT_CONTROLLER (controller));

  event = g_strdup_printf ("KEYSYM %u %i", keysym, state);
  valent_test_event_push (event);
}

static void
valent_test_input_controller_pointer_axis (ValentInputController *controller,
                                           double                 dx,
                                           double                 dy)
{
  char *event;

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_TEST_INPUT_CONTROLLER (controller));
  g_assert (dx != 0.0 || dy != 0.0);

  event = g_strdup_printf ("POINTER AXIS %.1f %.1f", dx, dy);
  valent_test_event_push (event);
}

static void
valent_test_input_controller_pointer_button (ValentInputController *controller,
                                             ValentPointerButton    button,
                                             gboolean               pressed)
{
  char *event;

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_TEST_INPUT_CONTROLLER (controller));
  g_assert (button > 0 && button < 8);

  event = g_strdup_printf ("POINTER BUTTON %u %i", button, pressed);
  valent_test_event_push (event);
}

static void
valent_test_input_controller_pointer_motion (ValentInputController *controller,
                                             double                 dx,
                                             double                 dy)
{
  char *event;

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_TEST_INPUT_CONTROLLER (controller));
  g_return_if_fail (dx != 0 || dy != 0);

  event = g_strdup_printf ("POINTER MOTION %.1f %.1f", dx, dy);
  valent_test_event_push (event);
}

static void
valent_test_input_controller_pointer_position (ValentInputController *controller,
                                               double                 x,
                                               double                 y)
{
  char *event;

  g_assert (VALENT_IS_INPUT_CONTROLLER (controller));
  g_assert (VALENT_IS_TEST_INPUT_CONTROLLER (controller));

  event = g_strdup_printf ("POINTER POSITION %.1f %.1f", x, y);
  valent_test_event_push (event);
}

static void
valent_input_controller_iface_init (ValentInputControllerInterface *iface)
{
  iface->keyboard_keysym = valent_test_input_controller_keyboard_keysym;
  iface->pointer_axis = valent_test_input_controller_pointer_axis;
  iface->pointer_button = valent_test_input_controller_pointer_button;
  iface->pointer_motion = valent_test_input_controller_pointer_motion;
  iface->pointer_position = valent_test_input_controller_pointer_position;
}

/*
 * GObject
 */
static void
valent_test_input_controller_class_init (ValentTestInputControllerClass *klass)
{
}

static void
valent_test_input_controller_init (ValentTestInputController *self)
{
}

