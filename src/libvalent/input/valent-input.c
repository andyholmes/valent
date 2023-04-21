// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input"

#include "config.h"

#include <glib-object.h>
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

G_DEFINE_FINAL_TYPE (ValentInput, valent_input, VALENT_TYPE_COMPONENT)


static ValentInput *default_input = NULL;


/*
 * ValentComponent
 */
static void
valent_input_bind_preferred (ValentComponent *component,
                             GObject         *extension)
{
  ValentInput *self = VALENT_INPUT (component);
  ValentInputAdapter *adapter = VALENT_INPUT_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_INPUT (self));
  g_assert (adapter == NULL || VALENT_IS_INPUT_ADAPTER (adapter));

  self->default_adapter = adapter;

  VALENT_EXIT;
}

static void
valent_input_class_init (ValentInputClass *klass)
{
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  component_class->bind_preferred = valent_input_bind_preferred;
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
                                    "plugin-domain", "input",
                                    "plugin-type",   VALENT_TYPE_INPUT_ADAPTER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_input),
                                 (gpointer)&default_input);
    }

  return default_input;
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
 * @button: a button
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @button.
 *
 * Since: 1.0
 */
void
valent_input_pointer_button (ValentInput  *input,
                             unsigned int  button,
                             gboolean      state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT (input));

  if G_LIKELY (input->default_adapter != NULL)
    valent_input_adapter_pointer_button (input->default_adapter, button, state);

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

