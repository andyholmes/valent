// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-input-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-input-adapter.h"
#include "valent-input-keydef.h"


/**
 * SECTION:valentinputadapter
 * @short_description: Interface for input adapters
 * @title: ValentInputAdapter
 * @stability: Unstable
 * @include: libvalent-input.h
 *
 * #ValentInputAdapter is a base class for plugins that provide a means to
 * simulate keyboard and pointer events on the host system on behalf of remote
 * devices.
 *
 * ## .plugin File ##
 *
 * Plugins require no special entries in the `.plugin` file, but may specify the
 * `X-InputAdapterPriority` field with an integer value. The implementation with
 * the lowest value will take precedence.
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
 * @pointer_position: the virtual function pointer for valent_input_adapter_pointer_position()
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
                                          ValentPointerButton  button,
                                          gboolean             state)
{
}

static void
valent_input_adapter_real_pointer_motion (ValentInputAdapter *adapter,
                                          double              dx,
                                          double              dy)
{
}

static void
valent_input_adapter_real_pointer_position (ValentInputAdapter *adapter,
                                            double              x,
                                            double              y)
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
  klass->pointer_position = valent_input_adapter_real_pointer_position;

  /**
   * ValentInputAdapter:plugin-info:
   *
   * The #PeasPluginInfo describing this adapter.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "Plugin Info",
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
 * @state: if pressed
 *
 * Simulate a keysym event for @keysym.
 */
void
valent_input_adapter_keyboard_keysym (ValentInputAdapter *adapter,
                                      unsigned int        keysym,
                                      gboolean            state)
{
  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  /* Silently ignore empty symbols */
  if G_UNLIKELY (keysym == 0)
    return;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->keyboard_keysym (adapter,
                                                             keysym,
                                                             state);
}

/**
 * valent_input_adapter_pointer_axis:
 * @adapter: a #ValentInputAdapter
 * @dx: relate movement on x-axis
 * @dy: relate movement on y-axis
 *
 * Simulate pointer movement (@dx, @dy). Implementations should handle any
 * necessary scaling.
 */
void
valent_input_adapter_pointer_axis (ValentInputAdapter *adapter,
                                   double              dx,
                                   double              dy)
{
  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  /* Silently ignore 0-delta motion */
  if G_UNLIKELY (dx == 0.0 && dy == 0.0)
    return;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_axis (adapter, dx, dy);
}

/**
 * valent_input_adapter_pointer_button:
 * @adapter: a #ValentInputAdapter
 * @button: a #ValentPointerBUtton
 * @state: a #ValentInputButtonState
 *
 * TODO
 */
void
valent_input_adapter_pointer_button (ValentInputAdapter  *adapter,
                                     ValentPointerButton  button,
                                     gboolean             state)
{
  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));
  g_return_if_fail (button > 0 && button < 9);

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_button (adapter,
                                                            button,
                                                            state);
}

/**
 * valent_input_adapter_pointer_motion:
 * @adapter: a #ValentInputAdapter
 * @dx: relate movement on x-axis
 * @dy: relate movement on y-axis
 *
 * Simulate pointer movement (@dx, @dy). Implementations should handle any
 * necessary scaling.
 */
void
valent_input_adapter_pointer_motion (ValentInputAdapter *adapter,
                                     double              dx,
                                     double              dy)
{
  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  /* Silently ignore 0-delta motion */
  if G_UNLIKELY (dx == 0.0 && dy == 0.0)
    return;

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_motion (adapter, dx, dy);
}

/**
 * valent_input_adapter_pointer_position:
 * @adapter: a #ValentInputAdapter
 * @x: position on x-axis
 * @y: position on y-axis
 *
 * Simulate absolute pointer movement (@x, @y). Implementations should handle
 * any necessary scaling.
 */
void
valent_input_adapter_pointer_position (ValentInputAdapter *adapter,
                                       double              x,
                                       double              y)
{
  g_return_if_fail (VALENT_IS_INPUT_ADAPTER (adapter));

  VALENT_INPUT_ADAPTER_GET_CLASS (adapter)->pointer_position (adapter, x, y);
}

