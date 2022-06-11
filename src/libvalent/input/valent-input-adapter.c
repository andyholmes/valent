// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-input-adapter.h"


/**
 * ValentInputAdapter:
 *
 * An abstract base class for virtual input devices.
 *
 * #ValentInputAdapter is a base class for plugins that provide an interface to
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
  PeasPluginInfo *plugin_info;
} ValentInputAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentInputAdapter, valent_input_adapter, G_TYPE_OBJECT)

/**
 * ValentInputAdapterClass:
 * @keyboard_keysym: the virtual function pointer for valent_input_adapter_keyboard_keysym()
 * @pointer_axis: the virtual function pointer for valent_input_adapter_pointer_axis()
 * @pointer_button: the virtual function pointer for valent_input_adapter_pointer_button()
 * @pointer_motion: the virtual function pointer for valent_input_adapter_pointer_motion()
 *
 * The virtual function table for #ValentInputAdapter.
 */

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/* LCOV_EXCL_START */
static void
valent_input_adapter_real_keyboard_keysym (ValentInputAdapter *adapter,
                                           unsigned int        keysym,
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
valent_input_adapter_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentInputAdapter *self = VALENT_INPUT_ADAPTER (object);
  ValentInputAdapterPrivate *priv = valent_input_adapter_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_input_adapter_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentInputAdapter *self = VALENT_INPUT_ADAPTER (object);
  ValentInputAdapterPrivate *priv = valent_input_adapter_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_input_adapter_class_init (ValentInputAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = valent_input_adapter_get_property;
  object_class->set_property = valent_input_adapter_set_property;

  klass->keyboard_keysym = valent_input_adapter_real_keyboard_keysym;
  klass->pointer_axis = valent_input_adapter_real_pointer_axis;
  klass->pointer_button = valent_input_adapter_real_pointer_button;
  klass->pointer_motion = valent_input_adapter_real_pointer_motion;

  /**
   * ValentInputAdapter:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this adapter.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info", NULL, NULL,
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_input_adapter_init (ValentInputAdapter *adapter)
{
}

/**
 * valent_input_adapter_keyboard_keysym:
 * @adapter: a #ValentInputAdapter
 * @keysym: a keysym
 * @state: %TRUE to press, or %FALSE to release
 *
 * Press or release @keysym.
 *
 * Since: 1.0
 */
void
valent_input_adapter_keyboard_keysym (ValentInputAdapter *adapter,
                                      unsigned int        keysym,
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
 * @adapter: a #ValentInputAdapter
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
  if G_UNLIKELY (dx == 0.0 && dy == 0.0)
    VALENT_EXIT;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_axis (adapter, dx, dy);

  VALENT_EXIT;
}

/**
 * valent_input_adapter_pointer_button:
 * @adapter: a #ValentInputAdapter
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
 * @adapter: a #ValentInputAdapter
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
  if G_UNLIKELY (dx == 0.0 && dy == 0.0)
    VALENT_EXIT;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_motion (adapter, dx, dy);

  VALENT_EXIT;
}

