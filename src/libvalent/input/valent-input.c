// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input"

#include "config.h"

#include <gdk/gdk.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-input.h"
#include "valent-input-adapter.h"


/**
 * ValentInput:
 *
 * A class for controlling pointer and keyboard devices.
 *
 * #ValentInput is an abstraction of virtual input devices, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.InputAdapter] to provide an interface to
 * control the pointer and keyboard.
 *
 * Since: 1.0
 */

struct _ValentInput
{
  ValentComponent     parent_instance;

  ValentInputAdapter *default_adapter;
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

  provider = valent_component_get_priority_provider (component,
                                                     "InputAdapterPriority");
  g_set_object (&self->default_adapter, VALENT_INPUT_ADAPTER (provider));

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

  provider = valent_component_get_priority_provider (component,
                                                     "InputAdapterPriority");
  g_set_object (&self->default_adapter, VALENT_INPUT_ADAPTER (provider));

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
 * Get the default [class@Valent.Input].
 *
 * Returns: (transfer none) (not nullable): a #ValentInput
 *
 * Since: 1.0
 */
ValentInput *
valent_input_get_default (void)
{
  if (default_input == NULL)
    {
      default_input = g_object_new (VALENT_TYPE_INPUT,
                                    "plugin-context", "input",
                                    "plugin-type",    VALENT_TYPE_INPUT_ADAPTER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_input),
                                 (gpointer)&default_input);
    }

  return default_input;
}

/**
 * valent_input_keyboard_action:
 * @input: a #ValentInput
 * @keysym: a GDK KeySym
 * @mask: a #GdkModifierType
 *
 * A convenience method to press and release @keysym with the modifiers locked
 * for @mask.
 *
 * Since: 1.0
 */
void
valent_input_keyboard_action (ValentInput     *input,
                              unsigned int     keysym,
                              GdkModifierType  mask)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));
  g_return_if_fail (keysym > 0);

  if (mask != 0)
    valent_input_keyboard_mask (input, mask, TRUE);

  valent_input_keyboard_keysym (input, keysym, TRUE);
  valent_input_keyboard_keysym (input, keysym, FALSE);

  if (mask != 0)
    valent_input_keyboard_mask (input, mask, FALSE);

  VALENT_EXIT;
}

/**
 * valent_input_keyboard_keysym:
 * @input: a #ValentInput
 * @keysym: a keysym
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @keysym.
 *
 * Since: 1.0
 */
void
valent_input_keyboard_keysym (ValentInput  *input,
                              unsigned int  keysym,
                              gboolean      state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_keyboard_keysym (input->default_adapter, keysym, state);

  VALENT_EXIT;
}

/**
 * valent_input_keyboard_mask:
 * @input: a #ValentInput
 * @mask: a #GdkModifierType
 * @lock: whether to lock modifiers
 *
 * A convenience method to lock or unlock the modifiers for @mask.
 *
 * Since: 1.0
 */
void
valent_input_keyboard_mask (ValentInput     *input,
                            GdkModifierType  mask,
                            gboolean         lock)
{
  VALENT_ENTRY;

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

  VALENT_EXIT;
}

/**
 * valent_input_pointer_axis:
 * @input: a #ValentInput
 * @dx: movement on x-axis
 * @dy: movement on y-axis
 *
 * Scroll the surface under the pointer (@dx, @dy), relative to its current
 * position.
 *
 * Since: 1.0
 */
void
valent_input_pointer_axis (ValentInput *input,
                           double       dx,
                           double       dy)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_axis (input->default_adapter, dx, dy);

  VALENT_EXIT;
}

/**
 * valent_input_pointer_button:
 * @input: a #ValentInput
 * @button: a #ValentPointerButton
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @button.
 *
 * Since: 1.0
 */
void
valent_input_pointer_button (ValentInput         *input,
                             ValentPointerButton  button,
                             gboolean             state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_button (input->default_adapter, button, state);

  VALENT_EXIT;
}

/**
 * valent_input_pointer_click:
 * @input: a #ValentInput
 * @button: a #ValentPointerButton
 *
 * A convenience method for pressing and releasing @button.
 *
 * Since: 1.0
 */
void
valent_input_pointer_click (ValentInput         *input,
                            ValentPointerButton  button)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    {
      valent_input_adapter_pointer_button (input->default_adapter,
                                           button,
                                           TRUE);
      valent_input_adapter_pointer_button (input->default_adapter,
                                           button,
                                           FALSE);
    }

  VALENT_EXIT;
}

/**
 * valent_input_pointer_motion:
 * @input: a #ValentInput
 * @dx: position on x-axis
 * @dy: position on y-axis
 *
 * Move the pointer (@dx, @dy), relative to its current position.
 *
 * Since: 1.0
 */
void
valent_input_pointer_motion (ValentInput *input,
                             double       dx,
                             double       dy)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_motion (input->default_adapter, dx, dy);

  VALENT_EXIT;
}

/**
 * valent_input_pointer_position:
 * @input: a #ValentInput
 * @x: position on x-axis
 * @y: position on y-axis
 *
 * Move the pointer to the absolute position (@x, @y).
 *
 * Since: 1.0
 */
void
valent_input_pointer_position (ValentInput *input,
                               double       x,
                               double       y)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_position (input->default_adapter, x, y);

  VALENT_EXIT;
}

