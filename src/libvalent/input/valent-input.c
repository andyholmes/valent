// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input"

#include "config.h"

#include <gdk/gdk.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-input.h"
#include "valent-input-controller.h"


/**
 * SECTION:valentinput
 * @short_description: Input Abstraction
 * @title: ValentInput
 * @stability: Unstable
 * @include: libvalent-input.h
 *
 * #ValentInput is an abstraction of input controllers, with a simple API
 * generally intended to be used by #ValentDevicePlugin implementations.
 *
 * Plugins can provide adapters for input controllers by subclassing the
 * #ValentInputController base class. The priority of input controllers is
 * determined by the `.plugin` file key `X-InputControllerPriority`.
 */

struct _ValentInput
{
  ValentComponent        parent_instance;

  ValentInputController *default_controller;
};

G_DEFINE_TYPE (ValentInput, valent_input, VALENT_TYPE_COMPONENT)


static ValentInput *default_input = NULL;


/*
 * ValentComponent
 */
static void
valent_input_extension_added (ValentComponent *component,
                              PeasExtension   *extension)
{
  ValentInput *self = VALENT_INPUT (component);
  PeasExtension *provider;

  VALENT_ENTRY;

  g_assert (VALENT_IS_INPUT (self));

  provider = valent_component_get_priority_provider (component, "InputControllerPriority");
  g_set_object (&self->default_controller, VALENT_INPUT_CONTROLLER (provider));

  VALENT_EXIT;
}

static void
valent_input_extension_removed (ValentComponent *component,
                                PeasExtension   *extension)
{
  ValentInput *self = VALENT_INPUT (component);
  PeasExtension *provider;

  VALENT_ENTRY;

  g_assert (VALENT_IS_INPUT (self));

  provider = valent_component_get_priority_provider (component, "InputControllerPriority");
  g_set_object (&self->default_controller, VALENT_INPUT_CONTROLLER (provider));

  VALENT_EXIT;
}

static void
valent_input_class_init (ValentInputClass *klass)
{
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  component_class->extension_added = valent_input_extension_added;
  component_class->extension_removed = valent_input_extension_removed;
}

static void
valent_input_init (ValentInput *self)
{
}

/**
 * valent_input_get_default:
 *
 * Get the default #ValentInput.
 *
 * Returns: (transfer none): The default input
 */
ValentInput *
valent_input_get_default (void)
{
  if (default_input == NULL)
    {
      default_input = g_object_new (VALENT_TYPE_INPUT,
                                    "plugin-context", "input",
                                    "plugin-type",    VALENT_TYPE_INPUT_CONTROLLER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_input), (gpointer) &default_input);
    }

  return default_input;
}

/**
 * valent_input_keyboard_action:
 * @input: a #ValentInput
 * @keysym: a GDK KeySym
 * @mask: a #GdkModifierType
 *
 * Simulate a press and release of @keysym with modifiers locked to @mask.
 */
void
valent_input_keyboard_action (ValentInput     *input,
                              unsigned int     keysym,
                              GdkModifierType  mask)
{
  g_return_if_fail (VALENT_IS_INPUT (input));
  g_return_if_fail (keysym > 0);

  if (mask != 0)
    valent_input_keyboard_mask (input, mask, TRUE);

  valent_input_keyboard_keysym (input, keysym, TRUE);
  valent_input_keyboard_keysym (input, keysym, FALSE);

  if (mask != 0)
    valent_input_keyboard_mask (input, mask, FALSE);
}

/**
 * valent_input_keyboard_keysym:
 * @input: a #ValentInput
 * @keysym: a keysym
 * @state: if pressed
 *
 * Simulate a keysym event for @keysym using the default #ValentInputController.
 */
void
valent_input_keyboard_keysym (ValentInput *input,
                              guint        keysym,
                              gboolean     state)
{
  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_controller != NULL)
    valent_input_controller_keyboard_keysym (input->default_controller, keysym, state);
  else
    g_debug ("[%s] No source available", G_STRFUNC);
}

/**
 * valent_input_keyboard_mask:
 * @input: a #ValentInput
 * @mask: a #GdkModifierType
 * @lock: whether to lock modifiers
 *
 * A convenience function that wraps valent_input_keyboard_keysym() to toggle the keysyms for @mask.
 */
void
valent_input_keyboard_mask (ValentInput     *input,
                            GdkModifierType  mask,
                            gboolean         lock)
{
  g_return_if_fail (VALENT_IS_INPUT (input));
  g_return_if_fail (mask > 0);

  if (mask & GDK_ALT_MASK)
    valent_input_keyboard_keysym (input, GDK_KEY_Alt_L, lock);

  if (mask & GDK_CONTROL_MASK)
    valent_input_keyboard_keysym (input, GDK_KEY_Control_L, lock);

  if (mask & GDK_SHIFT_MASK)
    valent_input_keyboard_keysym (input, GDK_KEY_Shift_L, lock);

  if (mask & GDK_SUPER_MASK)
    valent_input_keyboard_keysym (input, GDK_KEY_Super_L, lock);
}

void
valent_input_pointer_axis (ValentInput *input,
                           double       dx,
                           double       dy)
{
  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_controller != NULL)
    valent_input_controller_pointer_axis (input->default_controller, dx, dy);
  else
    g_debug ("[%s] No source available", G_STRFUNC);
}

/**
 * valent_input_pointer_button:
 * @input: a #ValentInput
 * @button: a #ValentPointeruUtton
 * @state: a state
 *
 * TODO
 */
void
valent_input_pointer_button (ValentInput         *input,
                             ValentPointerButton  button,
                             gboolean             state)
{
  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_controller != NULL)
    valent_input_controller_pointer_button (input->default_controller, button, state);
  else
    g_debug ("[%s] No source available", G_STRFUNC);
}

/**
 * valent_input_pointer_click:
 * @input: a #ValentInput
 * @button: a #ValentPointerButton
 *
 * A convenience function for pressing and releasing a pointer @button.
 */
void
valent_input_pointer_click (ValentInput         *input,
                            ValentPointerButton  button)
{
  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_controller != NULL)
    {
      valent_input_controller_pointer_button (input->default_controller, button, TRUE);
      valent_input_controller_pointer_button (input->default_controller, button, FALSE);
    }
  else
    g_debug ("[%s] No input controller available", G_STRFUNC);
}

/**
 * valent_input_pointer_motion:
 * @input: a #ValentInput
 * @dx: relate movement on x-axis
 * @dy: relate movement on y-axis
 *
 * Simulate pointer movement (@dx, @dy). Implementations handle any necessary scaling.
 */
void
valent_input_pointer_motion (ValentInput *input,
                             double       dx,
                             double       dy)
{
  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_controller != NULL)
    valent_input_controller_pointer_motion (input->default_controller, dx, dy);
  else
    g_debug ("[%s] No input controller available", G_STRFUNC);
}

/**
 * valent_input_pointer_position:
 * @input: a #ValentInput
 * @x: position on x-axis
 * @y: position on y-axis
 *
 * Simulate absolute pointer movement (@x, @y). Implementations handle any necessary scaling.
 */
void
valent_input_pointer_position (ValentInput *input,
                               double       x,
                               double       y)
{
  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_controller != NULL)
    valent_input_controller_pointer_position (input->default_controller, x, y);
  else
    g_debug ("[%s] No input controller available", G_STRFUNC);
}

