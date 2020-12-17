// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input-controller"

#include "config.h"

#include <gdk/gdk.h>
#include <libvalent-core.h>

#include "valent-input-controller.h"
#include "valent-input-keydef.h"


/**
 * SECTION:valent-input-controller
 * @short_description: Interface for virtual input controllers
 * @title: ValentInputController
 * @stability: Unstable
 * @include: libvalent-input.h
 *
 * #ValentInputController is an interface for virtual input controllers.
 * Implementations must override valent_input_controller_keyboard_keysym(),
 * valent_input_controller_pointer_button() and valent_input_controller_pointer_motion()
 * methods.
 *
 * There are additional interface methods that may be overridden if the
 * implementation can perform them more efficiently, otherwise they will be
 * approximated with the required methods.
 */

G_DEFINE_INTERFACE (ValentInputController, valent_input_controller, G_TYPE_OBJECT)

/**
 * ValentInputControllerInterface:
 * @keyboard_keysym: the virtual function pointer for valent_input_controller_keyboard_keysym()
 * @pointer_axis: the virtual function pointer for valent_input_controller_pointer_axis()
 * @pointer_button: the virtual function pointer for valent_input_controller_pointer_button()
 * @pointer_motion: the virtual function pointer for valent_input_controller_pointer_motion()
 * @pointer_position: the virtual function pointer for valent_input_controller_pointer_position()
 *
 * The virtual function table for #ValentInputController.
 */

static void
valent_input_controller_default_init (ValentInputControllerInterface *iface)
{
}

/**
 * valent_input_controller_keyboard_keysym:
 * @controller: a #ValentInputController
 * @keysym: a keysym
 * @state: if pressed
 *
 * Simulate a keysym event for @keysym.
 */
void
valent_input_controller_keyboard_keysym (ValentInputController *controller,
                                         guint                  keysym,
                                         gboolean               state)
{
  ValentInputControllerInterface *iface;

  g_return_if_fail (VALENT_IS_INPUT_CONTROLLER (controller));

  /* Silently ignore empty symbols */
  if (keysym <= 0)
    return;

  iface = VALENT_INPUT_CONTROLLER_GET_IFACE (controller);
  g_return_if_fail (iface->keyboard_keysym);

  iface->keyboard_keysym (controller, keysym, state);
}

/**
 * valent_input_controller_pointer_axis:
 * @controller: a #ValentInputController
 * @dx: relate movement on x-axis
 * @dy: relate movement on y-axis
 *
 * Simulate pointer movement (@dx, @dy). Implementations should handle any necessary scaling.
 */
void
valent_input_controller_pointer_axis (ValentInputController *controller,
                                      double                 dx,
                                      double                 dy)
{
  ValentInputControllerInterface *iface;

  g_return_if_fail (VALENT_IS_INPUT_CONTROLLER (controller));

  /* Silently ignore 0-delta motion */
  if (dx == 0.0 && dy == 0.0)
    return;

  iface = VALENT_INPUT_CONTROLLER_GET_IFACE (controller);
  g_return_if_fail (iface->pointer_axis);

  iface->pointer_axis (controller, dx, dy);
}

/**
 * valent_input_controller_pointer_button:
 * @controller: a #ValentInputController
 * @button: a #ValentPointerBUtton
 * @state: a #ValentInputButtonState
 *
 * TODO
 */
void
valent_input_controller_pointer_button (ValentInputController *controller,
                                        ValentPointerButton    button,
                                        gboolean               state)
{
  ValentInputControllerInterface *iface;

  g_return_if_fail (VALENT_IS_INPUT_CONTROLLER (controller));
  g_return_if_fail (button > 0 && button < 9);

  iface = VALENT_INPUT_CONTROLLER_GET_IFACE (controller);
  g_return_if_fail (iface->pointer_button);

  iface->pointer_button (controller, button, state);
}

/**
 * valent_input_controller_pointer_motion:
 * @controller: a #ValentInputController
 * @dx: relate movement on x-axis
 * @dy: relate movement on y-axis
 *
 * Simulate pointer movement (@dx, @dy). Implementations should handle any necessary scaling.
 */
void
valent_input_controller_pointer_motion (ValentInputController *controller,
                                        double                 dx,
                                        double                 dy)
{
  ValentInputControllerInterface *iface;

  g_return_if_fail (VALENT_IS_INPUT_CONTROLLER (controller));

  /* Silently ignore 0-delta motion */
  if (dx == 0.0 && dy == 0.0)
    return;

  iface = VALENT_INPUT_CONTROLLER_GET_IFACE (controller);
  g_return_if_fail (iface->pointer_motion);

  iface->pointer_motion (controller, dx, dy);
}

/**
 * valent_input_controller_pointer_position:
 * @controller: a #ValentInputController
 * @x: position on x-axis
 * @y: position on y-axis
 *
 * Simulate absolute pointer movement (@x, @y). Implementations should handle any necessary scaling.
 */
void
valent_input_controller_pointer_position (ValentInputController *controller,
                                          double                 x,
                                          double                 y)
{
  ValentInputControllerInterface *iface;

  g_return_if_fail (VALENT_IS_INPUT_CONTROLLER (controller));

  iface = VALENT_INPUT_CONTROLLER_GET_IFACE (controller);
  g_return_if_fail (iface->pointer_position);

  iface->pointer_position (controller, x, y);
}

