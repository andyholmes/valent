// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer-adapter"

#include "config.h"

#include <libpeas/peas.h>

#include "valent-mixer.h"
#include "valent-mixer-adapter.h"


/**
 * ValentMixerAdapter
 *
 * An abstract base class for audio mixers.
 *
 * #ValentMixerAdapter is a base class for plugins that provide an interface to
 * audio mixers and volume control. This usually means monitoring the available
 * input and output streams, changing properties on those streams, and selecting
 * which are the active input and output.
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-MixerAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */

typedef struct
{
  PeasPluginInfo *plugin_info;

  GPtrArray      *inputs;
  GPtrArray      *outputs;
} ValentMixerAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentMixerAdapter, valent_mixer_adapter, G_TYPE_OBJECT)


/**
 * ValentMixerAdapterClass:
 * @get_default_input: the virtual function pointer for valent_mixer_adapter_get_default_input()
 * @set_default_input: the virtual function pointer for valent_mixer_adapter_set_default_input()
 * @get_default_output: the virtual function pointer for valent_mixer_adapter_get_default_output()
 * @set_default_output: the virtual function pointer for valent_mixer_adapter_set_default_output()
 * @stream_added: the class closure for the #ValentMixerAdapter::stream-added signal
 * @stream_changed: the class closure for the #ValentMixerAdapter::stream-changed signal
 * @stream_removed: the class closure for the #ValentMixerAdapter::stream-removed signal
 *
 * The virtual function table for #ValentMixerAdapter.
 */

enum {
  PROP_0,
  PROP_DEFAULT_INPUT,
  PROP_DEFAULT_OUTPUT,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  STREAM_ADDED,
  STREAM_CHANGED,
  STREAM_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static GQuark input_detail = 0;
static GQuark output_detail = 0;


/* LCOV_EXCL_START */
static void
valent_mixer_adapter_real_stream_added (ValentMixerAdapter *adapter,
                                        ValentMixerStream  *stream)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    g_ptr_array_add (priv->inputs, g_object_ref (stream));
  else
    g_ptr_array_add (priv->outputs, g_object_ref (stream));
}

static void
valent_mixer_adapter_real_stream_changed (ValentMixerAdapter *adapter,
                                          ValentMixerStream  *stream)
{
}

static void
valent_mixer_adapter_real_stream_removed (ValentMixerAdapter *adapter,
                                          ValentMixerStream  *stream)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    g_ptr_array_remove (priv->inputs, stream);
  else
    g_ptr_array_remove (priv->outputs, stream);
}

static ValentMixerStream *
valent_mixer_adapter_real_get_default_input (ValentMixerAdapter *adapter)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));

  return NULL;
}

static void
valent_mixer_adapter_real_set_default_input (ValentMixerAdapter *adapter,
                                             ValentMixerStream  *stream)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
}

static ValentMixerStream *
valent_mixer_adapter_real_get_default_output (ValentMixerAdapter *adapter)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));

  return NULL;
}

static void
valent_mixer_adapter_real_set_default_output (ValentMixerAdapter *adapter,
                                              ValentMixerStream  *stream)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_mixer_adapter_dispose (GObject *object)
{
  ValentMixerAdapter *self = VALENT_MIXER_ADAPTER (object);
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  g_clear_pointer (&priv->inputs, g_ptr_array_unref);
  g_clear_pointer (&priv->outputs, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_mixer_adapter_parent_class)->dispose (object);
}

static void
valent_mixer_adapter_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentMixerAdapter *self = VALENT_MIXER_ADAPTER (object);
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEFAULT_INPUT:
      g_value_set_object (value, valent_mixer_adapter_get_default_input (self));
      break;

    case PROP_DEFAULT_OUTPUT:
      g_value_set_object (value, valent_mixer_adapter_get_default_output (self));
      break;

    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_adapter_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentMixerAdapter *self = VALENT_MIXER_ADAPTER (object);
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEFAULT_INPUT:
      valent_mixer_adapter_set_default_input (self, g_value_get_object (value));
      break;

    case PROP_DEFAULT_OUTPUT:
      valent_mixer_adapter_set_default_output (self, g_value_get_object (value));
      break;

    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_adapter_class_init (ValentMixerAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_mixer_adapter_dispose;
  object_class->get_property = valent_mixer_adapter_get_property;
  object_class->set_property = valent_mixer_adapter_set_property;

  klass->get_default_input = valent_mixer_adapter_real_get_default_input;
  klass->set_default_input = valent_mixer_adapter_real_set_default_input;
  klass->get_default_output = valent_mixer_adapter_real_get_default_output;
  klass->set_default_output = valent_mixer_adapter_real_set_default_output;
  klass->stream_added = valent_mixer_adapter_real_stream_added;
  klass->stream_changed = valent_mixer_adapter_real_stream_changed;
  klass->stream_removed = valent_mixer_adapter_real_stream_removed;

  /**
   * ValentMixerAdapter:default-input: (getter get_default_input) (setter set_default_input)
   *
   * The active input stream.
   *
   * Implementations should emit [signal@GObject.Object::notify] for this
   * property when the default stream changes.
   *
   * Since: 1.0
   */
  properties [PROP_DEFAULT_INPUT] =
    g_param_spec_object ("default-input", NULL, NULL,
                         VALENT_TYPE_MIXER_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerAdapter:default-output: (getter get_default_output) (setter set_default_output)
   *
   * The active output stream.
   *
   * Implementations should emit [signal@GObject.Object::notify] for this
   * property when the default stream changes.
   *
   * Since: 1.0
   */
  properties [PROP_DEFAULT_OUTPUT] =
    g_param_spec_object ("default-output", NULL, NULL,
                         VALENT_TYPE_MIXER_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerAdapter:plugin-info:
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

  /**
   * ValentMixerAdapter::stream-added:
   * @adapter: a #ValentMixerAdapter
   * @stream: a #ValentMixerStream
   *
   * Emitted when a new stream is added to @adapter.
   *
   * Implementations of #ValentMixerAdapter must chain-up if they override
   * [vfunc@Valent.MixerAdapter.stream_added].
   *
   * Since: 1.0
   */
  signals [STREAM_ADDED] =
    g_signal_new ("stream-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (ValentMixerAdapterClass, stream_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMixerAdapter::stream-changed:
   * @adapter: a #ValentMixerAdapter
   * @stream: a #ValentMixerStream
   *
   * Emitted when a stream is changed that belongs to @adapter.
   *
   * Since: 1.0
   */
  signals [STREAM_CHANGED] =
    g_signal_new ("stream-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (ValentMixerAdapterClass, stream_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMixerAdapter::stream-removed:
   * @adapter: a #ValentMixerAdapter
   * @stream: a #ValentMixerStream
   *
   * Emitted when a stream is removed from @adapter.
   *
   * Implementations of #ValentMixerAdapter must chain-up if they override
   * [vfunc@Valent.MixerAdapter.stream_removed].
   *
   * Since: 1.0
   */
  signals [STREAM_REMOVED] =
    g_signal_new ("stream-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (ValentMixerAdapterClass, stream_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /* Signal Details */
  input_detail = g_quark_from_static_string ("input");
  output_detail = g_quark_from_static_string ("output");
}

static void
valent_mixer_adapter_init (ValentMixerAdapter *self)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  priv->inputs = g_ptr_array_new_with_free_func (g_object_unref);
  priv->outputs = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_mixer_adapter_emit_stream_added:
 * @adapter: a #ValentMixerAdapter
 * @stream: a #ValentMixerStream
 *
 * Emit [signal@Valent.MixerAdapter::stream-added] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.MixerAdapter].
 *
 * Since: 1.0
 */
void
valent_mixer_adapter_emit_stream_added (ValentMixerAdapter *adapter,
                                        ValentMixerStream  *stream)
{
  GQuark detail = 0;

  g_return_if_fail (VALENT_IS_MIXER_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    detail = input_detail;
  else
    detail = output_detail;

  g_signal_emit (G_OBJECT (adapter), signals [STREAM_ADDED], detail, stream);
}

/**
 * valent_mixer_adapter_emit_stream_changed:
 * @adapter: a #ValentMixerAdapter
 * @stream: a #ValentMixerStream
 *
 * Emit [signal@Valent.MixerAdapter::stream-changed] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.MixerAdapter].
 *
 * Since: 1.0
 */
void
valent_mixer_adapter_emit_stream_changed (ValentMixerAdapter *adapter,
                                          ValentMixerStream  *stream)
{
  GQuark detail = 0;

  g_return_if_fail (VALENT_IS_MIXER_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    detail = input_detail;
  else
    detail = output_detail;

  g_signal_emit (G_OBJECT (adapter), signals [STREAM_CHANGED], detail, stream);
}

/**
 * valent_mixer_adapter_emit_stream_removed:
 * @adapter: a #ValentMixerAdapter
 * @stream: a #ValentMixerStream
 *
 * Emit [signal@Valent.MixerAdapter::stream-removed] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.MixerAdapter].
 *
 * Since: 1.0
 */
void
valent_mixer_adapter_emit_stream_removed (ValentMixerAdapter *adapter,
                                          ValentMixerStream  *stream)
{
  GQuark detail = 0;

  g_return_if_fail (VALENT_IS_MIXER_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    detail = input_detail;
  else
    detail = output_detail;

  g_signal_emit (G_OBJECT (adapter), signals [STREAM_REMOVED], detail, stream);
}

/**
 * valent_mixer_adapter_get_default_input: (get-property default-input) (virtual get_default_input)
 * @adapter: a #ValentMixerAdapter
 *
 * Get the default input stream for @adapter.
 *
 * Returns: (transfer none): a #ValentMixerStream
 *
 * Since: 1.0
 */
ValentMixerStream *
valent_mixer_adapter_get_default_input (ValentMixerAdapter *adapter)
{
  ValentMixerStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_ADAPTER (adapter), NULL);

  ret = VALENT_MIXER_ADAPTER_GET_CLASS (adapter)->get_default_input (adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_adapter_set_default_input: (set-property default-input) (virtual set_default_input)
 * @adapter: a #ValentMixerAdapter
 * @stream: a #ValentMixerStream
 *
 * Set the default input stream for @adapter to @stream.
 *
 * Since: 1.0
 */
void
valent_mixer_adapter_set_default_input (ValentMixerAdapter *adapter,
                                        ValentMixerStream  *stream)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MIXER_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  VALENT_MIXER_ADAPTER_GET_CLASS (adapter)->set_default_input (adapter, stream);

  VALENT_EXIT;
}

/**
 * valent_mixer_adapter_get_default_output: (get-property default-output) (virtual get_default_output)
 * @adapter: a #ValentMixerAdapter
 *
 * Get the default output stream for @adapter.
 *
 * Returns: (transfer none): a #ValentMixerStream
 *
 * Since: 1.0
 */
ValentMixerStream *
valent_mixer_adapter_get_default_output (ValentMixerAdapter *adapter)
{
  ValentMixerStream *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_ADAPTER (adapter), NULL);

  ret = VALENT_MIXER_ADAPTER_GET_CLASS (adapter)->get_default_output (adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_adapter_set_default_output: (set-property default-output) (virtual set_default_output)
 * @adapter: a #ValentMixerAdapter
 * @stream: a #ValentMixerStream
 *
 * Set the default output stream for @adapter to @stream.
 *
 * Since: 1.0
 */
void
valent_mixer_adapter_set_default_output (ValentMixerAdapter *adapter,
                                         ValentMixerStream  *stream)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MIXER_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  VALENT_MIXER_ADAPTER_GET_CLASS (adapter)->set_default_output (adapter, stream);

  VALENT_EXIT;
}

/**
 * valent_mixer_adapter_get_inputs:
 * @adapter: a #ValentMixerAdapter
 *
 * Get a list of the input streams managed by @adapter.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream) (not nullable):
 *   a #GPtrArray of #ValentMixerStream
 *
 * Since: 1.0
 */
GPtrArray *
valent_mixer_adapter_get_inputs (ValentMixerAdapter *adapter)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (adapter);
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_ADAPTER (adapter), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < priv->inputs->len; i++)
    g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (priv->inputs, i)));

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_adapter_get_outputs:
 * @adapter: a #ValentMixerAdapter
 *
 * Get a list of the output streams managed by @adapter.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream) (not nullable):
 *   a #GPtrArray of #ValentMixerStream
 *
 * Since: 1.0
 */
GPtrArray *
valent_mixer_adapter_get_outputs (ValentMixerAdapter *adapter)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (adapter);
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_ADAPTER (adapter), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < priv->outputs->len; i++)
    g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (priv->outputs, i)));

  VALENT_RETURN (ret);
}

