// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-input-adapter.h"


/**
 * ValentInputAdapter:
 *
 * An abstract base class for virtual input devices.
 *
 * `ValentInputAdapter` is a base class for plugins that provide an interface to
 * the pointer and keyboard. This usually means simulating pointer and keyboard
 * events on the host system.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-InputAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  uint8_t  active : 1;
} ValentInputAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentInputAdapter, valent_input_adapter, VALENT_TYPE_EXTENSION)

/**
 * ValentInputAdapterClass:
 * @keyboard_keysym: the virtual function pointer for valent_input_adapter_keyboard_keysym()
 * @pointer_axis: the virtual function pointer for valent_input_adapter_pointer_axis()
 * @pointer_button: the virtual function pointer for valent_input_adapter_pointer_button()
 * @pointer_motion: the virtual function pointer for valent_input_adapter_pointer_motion()
 *
 * The virtual function table for `ValentInputAdapter`.
 */


/* LCOV_EXCL_START */
static void
valent_input_adapter_real_keyboard_keysym (ValentInputAdapter *adapter,
                                           uint32_t            keysym,
                                           gboolean            state)
{
}

static void
valent_input_adapter_real_pointer_axis (ValentInputAdapter *adapter,
                                        double              dx,
                                        double              dy)
{
}

static void
valent_input_adapter_real_pointer_button (ValentInputAdapter  *adapter,
                                          unsigned int         button,
                                          gboolean             state)
{
}

static void
valent_input_adapter_real_pointer_motion (ValentInputAdapter *adapter,
                                          double              dx,
                                          double              dy)
{
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_input_adapter_class_init (ValentInputAdapterClass *klass)
{
  klass->keyboard_keysym = valent_input_adapter_real_keyboard_keysym;
  klass->pointer_axis = valent_input_adapter_real_pointer_axis;
  klass->pointer_button = valent_input_adapter_real_pointer_button;
  klass->pointer_motion = valent_input_adapter_real_pointer_motion;
}

static void
valent_input_adapter_init (ValentInputAdapter *adapter)
{
}

/**
 * valent_input_adapter_keyboard_keysym:
 * @adapter: a `ValentInputAdapter`
 * @keysym: a keysym
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @keysym.
 *
 * Since: 1.0
 */
void
valent_input_adapter_keyboard_keysym (ValentInputAdapter *adapter,
                                      uint32_t            keysym,
                                      gboolean            state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  /* Silently ignore empty symbols */
  if G_UNLIKELY (keysym == 0)
    VALENT_EXIT;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->keyboard_keysym (adapter,
                                                             keysym,
                                                             state);

  VALENT_EXIT;
}

/**
 * valent_input_adapter_pointer_axis:
 * @adapter: a `ValentInputAdapter`
 * @dx: movement on x-axis
 * @dy: movement on y-axis
 *
 * Scroll the surface under the pointer (@dx, @dy), relative to its current
 * position.
 *
 * Implementations should handle any necessary scaling.
 *
 * Since: 1.0
 */
void
valent_input_adapter_pointer_axis (ValentInputAdapter *adapter,
                                   double              dx,
                                   double              dy)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  /* Silently ignore 0-delta motion */
  if G_UNLIKELY (G_APPROX_VALUE (dx, 0.0, 0.01) && G_APPROX_VALUE (dy, 0.0, 0.01))
    VALENT_EXIT;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_axis (adapter, dx, dy);

  VALENT_EXIT;
}

/**
 * valent_input_adapter_pointer_button:
 * @adapter: a `ValentInputAdapter`
 * @button: a button number
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @button.
 *
 * Since: 1.0
 */
void
valent_input_adapter_pointer_button (ValentInputAdapter *adapter,
                                     unsigned int        button,
                                     gboolean            state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_button (adapter,
                                                            button,
                                                            state);

  VALENT_EXIT;
}

/**
 * valent_input_adapter_pointer_motion:
 * @adapter: a `ValentInputAdapter`
 * @dx: movement on x-axis
 * @dy: movement on y-axis
 *
 * Move the pointer (@dx, @dy), relative to its current position.
 *
 * Implementation should handle any necessary scaling
 *
 * Since: 1.0
 */
void
valent_input_adapter_pointer_motion (ValentInputAdapter *adapter,
                                     double              dx,
                                     double              dy)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  /* Silently ignore 0-delta motion */
  if G_UNLIKELY (G_APPROX_VALUE (dx, 0.0, 0.01) && G_APPROX_VALUE (dy, 0.0, 0.01))
    VALENT_EXIT;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_motion (adapter, dx, dy);

  VALENT_EXIT;
}

