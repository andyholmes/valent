// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer-control"

#include "config.h"

#include <libpeas/peas.h>

#include "valent-mixer.h"
#include "valent-mixer-control.h"


/**
 * SECTION:valentmixercontrol
 * @short_description: Base class for mixer controls
 * @title: ValentMixerControl
 * @stability: Unstable
 * @include: libvalent-mixer.h
 *
 * #ValentMixerControl is an base class for audio mixer controls.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;

  GPtrArray      *inputs;
  GPtrArray      *outputs;
} ValentMixerControlPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentMixerControl, valent_mixer_control, G_TYPE_OBJECT)


/**
 * ValentMixerControlClass:
 * @get_default_input: the virtual function pointer for valent_mixer_control_get_default_input()
 * @get_default_output: the virtual function pointer for valent_mixer_control_get_default_output()
 * @stream_added: the class closure for the #ValentMixerControl::stream-added signal
 * @stream_changed: the class closure for the #ValentMixerControl::stream-changed signal
 * @stream_removed: the class closure for the #ValentMixerControl::stream-removed signal
 *
 * The virtual function table for #ValentMixerControl.
 */

enum {
  PROP_0,
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

/* Stream Type Quarks */
G_DEFINE_QUARK (input, valent_mixer_stream_input);
G_DEFINE_QUARK (output, valent_mixer_stream_output);


/* LCOV_EXCL_START */
static void
valent_mixer_control_real_stream_added (ValentMixerControl *control,
                                        ValentMixerStream  *stream)
{
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (control);
  ValentMixerStreamFlags flags;

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    g_ptr_array_add (priv->inputs, g_object_ref (stream));

  if (flags & VALENT_MIXER_STREAM_SINK)
    g_ptr_array_add (priv->outputs, g_object_ref (stream));
}

static void
valent_mixer_control_real_stream_changed (ValentMixerControl *control,
                                          ValentMixerStream  *stream)
{
}

static void
valent_mixer_control_real_stream_removed (ValentMixerControl *control,
                                          ValentMixerStream  *stream)
{
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (control);
  ValentMixerStreamFlags flags;

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    g_ptr_array_remove (priv->inputs, stream);

  if (flags & VALENT_MIXER_STREAM_SINK)
    g_ptr_array_remove (priv->outputs, stream);
}

static ValentMixerStream *
valent_mixer_control_real_get_default_input (ValentMixerControl *control)
{
  g_assert (VALENT_IS_MIXER_CONTROL (control));

  return NULL;
}

static ValentMixerStream *
valent_mixer_control_real_get_default_output (ValentMixerControl *control)
{
  g_assert (VALENT_IS_MIXER_CONTROL (control));

  return NULL;
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_mixer_control_dispose (GObject *object)
{
  ValentMixerControl *self = VALENT_MIXER_CONTROL (object);
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (self);

  g_clear_pointer (&priv->inputs, g_ptr_array_unref);
  g_clear_pointer (&priv->outputs, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_mixer_control_parent_class)->dispose (object);
}

static void
valent_mixer_control_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentMixerControl *self = VALENT_MIXER_CONTROL (object);
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (self);

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
valent_mixer_control_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentMixerControl *self = VALENT_MIXER_CONTROL (object);
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (self);

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
valent_mixer_control_class_init (ValentMixerControlClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_mixer_control_dispose;
  object_class->get_property = valent_mixer_control_get_property;
  object_class->set_property = valent_mixer_control_set_property;

  klass->get_default_input = valent_mixer_control_real_get_default_input;
  klass->get_default_output = valent_mixer_control_real_get_default_output;
  klass->stream_added = valent_mixer_control_real_stream_added;
  klass->stream_changed = valent_mixer_control_real_stream_changed;
  klass->stream_removed = valent_mixer_control_real_stream_removed;

  /**
   * ValentMixerControl:plugin-info:
   *
   * The #PeasPluginInfo describing this mixer control.
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

  /**
   * ValentMixerControl::stream-added:
   * @control: a #ValentMixerControl
   * @stream: a #ValentMixerStream
   *
   * The "stream-added" signal is emitted when a new stream is added to the
   * #ValentMixerControl.
   */
  signals [STREAM_ADDED] =
    g_signal_new ("stream-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (ValentMixerControlClass, stream_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMixerControl::stream-changed:
   * @control: a #ValentMixerControl
   * @stream: a #ValentMixerStream
   *
   * The "stream-changed" signal is emitted when a stream is changed from the
   * #ValentMixerControl.
   */
  signals [STREAM_CHANGED] =
    g_signal_new ("stream-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (ValentMixerControlClass, stream_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMixerControl::stream-removed:
   * @control: a #ValentMixerControl
   * @stream: a #ValentMixerStream
   *
   * The "stream-removed" signal is emitted when a new stream is removed from
   * the #ValentMixerControl.
   */
  signals [STREAM_REMOVED] =
    g_signal_new ("stream-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (ValentMixerControlClass, stream_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_mixer_control_init (ValentMixerControl *self)
{
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (self);

  priv->inputs = g_ptr_array_new_with_free_func (g_object_unref);
  priv->outputs = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_mixer_control_get_default_input:
 * @control: a #ValentMixerControl
 *
 * Get the default input stream for @control.
 *
 * Returns: (transfer none): a #ValentMixerStream
 */
ValentMixerStream *
valent_mixer_control_get_default_input (ValentMixerControl *control)
{
  g_return_val_if_fail (VALENT_IS_MIXER_CONTROL (control), NULL);

  return VALENT_MIXER_CONTROL_GET_CLASS (control)->get_default_input (control);
}

/**
 * valent_mixer_control_get_default_output:
 * @control: a #ValentMixerControl
 *
 * Get the default output stream for @control.
 *
 * Returns: (transfer none): a #ValentMixerStream
 */
ValentMixerStream *
valent_mixer_control_get_default_output (ValentMixerControl *control)
{
  g_return_val_if_fail (VALENT_IS_MIXER_CONTROL (control), NULL);

  return VALENT_MIXER_CONTROL_GET_CLASS (control)->get_default_output (control);
}

/**
 * valent_mixer_control_get_inputs:
 * @control: a #ValentMixerControl
 *
 * Get a list of the input streams managed by @control.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream) (not nullable):
 *   a #GPtrArray of #ValentMixerStream
 */
GPtrArray *
valent_mixer_control_get_inputs (ValentMixerControl *control)
{
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (control);
  GPtrArray *streams;

  g_return_val_if_fail (VALENT_IS_MIXER_CONTROL (control), NULL);

  streams = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < priv->inputs->len; i++)
    g_ptr_array_add (streams, g_object_ref (g_ptr_array_index (priv->inputs, i)));

  return streams;
}

/**
 * valent_mixer_control_get_outputs:
 * @control: a #ValentMixerControl
 *
 * Get a list of the output streams managed by @control.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream) (not nullable):
 *   a #GPtrArray of #ValentMixerStream
 */
GPtrArray *
valent_mixer_control_get_outputs (ValentMixerControl *control)
{
  ValentMixerControlPrivate *priv = valent_mixer_control_get_instance_private (control);
  GPtrArray *streams;

  g_return_val_if_fail (VALENT_IS_MIXER_CONTROL (control), NULL);

  streams = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < priv->outputs->len; i++)
    g_ptr_array_add (streams, g_object_ref (g_ptr_array_index (priv->outputs, i)));

  return streams;
}

/**
 * valent_mixer_control_emit_stream_added:
 * @control: a #ValentMixerControl
 * @stream: a #ValentMixerStream
 *
 * Emit the #ValentMixerControl::stream-added signal. This should only be called
 * by implementations after @stream has been added.
 */
void
valent_mixer_control_emit_stream_added (ValentMixerControl *control,
                                        ValentMixerStream  *stream)
{
  ValentMixerStreamFlags flags;
  guint detail = 0;

  g_return_if_fail (VALENT_IS_MIXER_CONTROL (control));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    detail = valent_mixer_stream_input_quark ();

  else if (flags & VALENT_MIXER_STREAM_SINK)
    detail = valent_mixer_stream_output_quark ();

  g_signal_emit (G_OBJECT (control), signals [STREAM_ADDED], detail, stream);
}

/**
 * valent_mixer_control_emit_stream_changed:
 * @control: a #ValentMixerControl
 * @stream: a #ValentMixerStream
 *
 * Emit the #ValentMixerControl::stream-changed signal. This should only be
 * called by implementations after @stream has changed.
 */
void
valent_mixer_control_emit_stream_changed (ValentMixerControl *control,
                                          ValentMixerStream  *stream)
{
  ValentMixerStreamFlags flags;
  guint detail = 0;

  g_return_if_fail (VALENT_IS_MIXER_CONTROL (control));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    detail = valent_mixer_stream_input_quark ();

  else if (flags & VALENT_MIXER_STREAM_SINK)
    detail = valent_mixer_stream_output_quark ();

  g_signal_emit (G_OBJECT (control), signals [STREAM_CHANGED], detail, stream);
}

/**
 * valent_mixer_control_emit_stream_removed:
 * @control: a #ValentMixerControl
 * @stream: a #ValentMixerStream
 *
 * Emit the #ValentMixerControl::stream-removed signal. This should only be
 * called by implementations after @stream has been removed.
 */
void
valent_mixer_control_emit_stream_removed (ValentMixerControl *control,
                                          ValentMixerStream  *stream)
{
  ValentMixerStreamFlags flags;
  guint detail = 0;

  g_return_if_fail (VALENT_IS_MIXER_CONTROL (control));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    detail = valent_mixer_stream_input_quark ();

  else if (flags & VALENT_MIXER_STREAM_SINK)
    detail = valent_mixer_stream_output_quark ();

  g_signal_emit (G_OBJECT (control), signals [STREAM_REMOVED], detail, stream);
}

