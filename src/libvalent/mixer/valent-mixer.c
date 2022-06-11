// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-component-private.h"
#include "valent-mixer.h"
#include "valent-mixer-adapter.h"
#include "valent-mixer-stream.h"


/**
 * ValentMixer:
 *
 * A class for monitoring and controlling the system volume.
 *
 * #ValentMixer is an abstraction of volume mixers, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.MixerAdapter] to provide an interface to
 * monitor and control audio streams.
 *
 * Since: 1.0
 */

struct _ValentMixer
{
  ValentComponent     parent_instance;

  ValentMixerAdapter *default_adapter;
  GPtrArray          *inputs;
  GPtrArray          *outputs;
};

G_DEFINE_TYPE (ValentMixer, valent_mixer, VALENT_TYPE_COMPONENT)

enum {
  PROP_0,
  PROP_DEFAULT_INPUT,
  PROP_DEFAULT_OUTPUT,
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


static ValentMixer *default_mixer = NULL;
static GQuark input_detail = 0;
static GQuark output_detail = 0;

/*
 * ValentMixerAdapter Callbacks
 */
static void
on_default_input_changed (ValentMixerAdapter *adapter,
                          GParamSpec         *pspec,
                          ValentMixer        *self)
{
  if (self->default_adapter != adapter)
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEFAULT_INPUT]);
}

static void
on_default_output_changed (ValentMixerAdapter *adapter,
                           GParamSpec         *pspec,
                           ValentMixer        *self)
{
  if (self->default_adapter != adapter)
    return;

  g_object_notify_by_pspec (G_OBJECT (self), properties [PROP_DEFAULT_OUTPUT]);
}

static inline void
valent_mixer_propagate_signal (ValentMixer       *self,
                               unsigned int       signal_id,
                               ValentMixerStream *stream)
{
  GQuark detail = 0;

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    detail = input_detail;
  else
    detail = output_detail;

  g_signal_emit (G_OBJECT (self), signal_id, detail, stream);
}

static void
on_stream_added (ValentMixerAdapter *adapter,
                 ValentMixerStream  *stream,
                 ValentMixer        *self)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    g_ptr_array_add (self->inputs, g_object_ref (stream));
  else
    g_ptr_array_add (self->outputs, g_object_ref (stream));

  valent_mixer_propagate_signal (self, signals [STREAM_ADDED], stream);
}

static void
on_stream_changed (ValentMixerAdapter *adapter,
                   ValentMixerStream  *stream,
                   ValentMixer        *self)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  valent_mixer_propagate_signal (self, signals [STREAM_CHANGED], stream);
}

static void
on_stream_removed (ValentMixerAdapter *adapter,
                   ValentMixerStream  *stream,
                   ValentMixer        *self)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
    g_ptr_array_remove (self->inputs, stream);
  else
    g_ptr_array_remove (self->outputs, stream);

  valent_mixer_propagate_signal (self, signals [STREAM_REMOVED], stream);
}

/*
 * ValentComponent
 */
static void
valent_mixer_enable_extension (ValentComponent *component,
                               PeasExtension   *extension)
{
  ValentMixer *self = VALENT_MIXER (component);
  ValentMixerAdapter *adapter = VALENT_MIXER_ADAPTER (extension);
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  PeasExtension *new_primary;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MIXER (self));
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));

  /* Add current streams */
  inputs = valent_mixer_adapter_get_inputs (adapter);

  for (unsigned int i = 0; i < inputs->len; i++)
    on_stream_added (adapter, g_ptr_array_index (inputs, i), self);

  outputs = valent_mixer_adapter_get_outputs (adapter);

  for (unsigned int i = 0; i < outputs->len; i++)
    on_stream_added (adapter, g_ptr_array_index (outputs, i), self);

  /* Watch for changes */
  g_signal_connect_object (adapter,
                           "notify::default-input",
                           G_CALLBACK (on_default_input_changed),
                           self, 0);

  g_signal_connect_object (adapter,
                           "notify::default-output",
                           G_CALLBACK (on_default_output_changed),
                           self, 0);

  g_signal_connect_object (adapter,
                           "stream-added",
                           G_CALLBACK (on_stream_added),
                           self, 0);

  g_signal_connect_object (adapter,
                           "stream-changed",
                           G_CALLBACK (on_stream_changed),
                           self, 0);

  g_signal_connect_object (adapter,
                           "stream-removed",
                           G_CALLBACK (on_stream_removed),
                           self, 0);

  /* Set default provider */
  new_primary = valent_component_get_primary (component);
  self->default_adapter = VALENT_MIXER_ADAPTER (new_primary);

  VALENT_EXIT;
}

static void
valent_mixer_disable_extension (ValentComponent *component,
                                PeasExtension   *extension)
{
  ValentMixer *self = VALENT_MIXER (component);
  ValentMixerAdapter *adapter = VALENT_MIXER_ADAPTER (extension);
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  PeasExtension *new_primary;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MIXER (self));
  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));

  /* Simulate stream removal */
  inputs = valent_mixer_adapter_get_inputs (adapter);

  for (unsigned int i = 0; i < inputs->len; i++)
    valent_mixer_adapter_emit_stream_removed (adapter, g_ptr_array_index (inputs, i));

  outputs = valent_mixer_adapter_get_outputs (adapter);

  for (unsigned int i = 0; i < outputs->len; i++)
    valent_mixer_adapter_emit_stream_removed (adapter, g_ptr_array_index (outputs, i));

  g_signal_handlers_disconnect_by_data (adapter, self);

  /* Set default provider */
  new_primary = valent_component_get_primary (component);
  self->default_adapter = VALENT_MIXER_ADAPTER (new_primary);

  VALENT_EXIT;
}


/*
 * GObject
 */
static void
valent_mixer_finalize (GObject *object)
{
  ValentMixer *self = VALENT_MIXER (object);

  g_clear_pointer (&self->inputs, g_ptr_array_unref);
  g_clear_pointer (&self->outputs, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_mixer_parent_class)->finalize (object);
}

static void
valent_mixer_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ValentMixer *self = VALENT_MIXER (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_INPUT:
      g_value_set_object (value, valent_mixer_get_default_input (self));
      break;

    case PROP_DEFAULT_OUTPUT:
      g_value_set_object (value, valent_mixer_get_default_output (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  ValentMixer *self = VALENT_MIXER (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_INPUT:
      valent_mixer_set_default_input (self, g_value_get_object (value));
      break;

    case PROP_DEFAULT_OUTPUT:
      valent_mixer_set_default_output (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_class_init (ValentMixerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_mixer_finalize;
  object_class->get_property = valent_mixer_get_property;
  object_class->set_property = valent_mixer_set_property;

  component_class->enable_extension = valent_mixer_enable_extension;
  component_class->disable_extension = valent_mixer_disable_extension;

  /**
   * ValentMixer:default-input: (getter get_default_input) (setter set_default_input)
   *
   * The active input stream.
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
   * ValentMixer:default-output: (getter get_default_output) (setter set_default_output)
   *
   * The active output stream.
   *
   * Since: 1.0
   */
  properties [PROP_DEFAULT_OUTPUT] =
    g_param_spec_object ("default-output", NULL, NULL,
                         VALENT_TYPE_MIXER_STREAM,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentMixer::stream-added:
   * @mixer: a #ValentMixer
   * @stream: a #ValentMixerStream
   *
   * Emitted when a stream is added to a [class@Valent.MixerAdapter] being
   * monitored by @mixer.
   *
   * Since: 1.0
   */
  signals [STREAM_ADDED] =
    g_signal_new ("stream-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMixer::stream-changed:
   * @mixer: a #ValentMixer
   * @stream: a #ValentMixerStream
   *
   * Emitted when a stream from a [class@Valent.MixerAdapter] being monitored by
   * @mixer changes.
   *
   * Since: 1.0
   */
  signals [STREAM_CHANGED] =
    g_signal_new ("stream-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MIXER_STREAM);
  g_signal_set_va_marshaller (signals [STREAM_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMixer::stream-removed:
   * @mixer: a #ValentMixer
   * @stream: a #ValentMixerStream
   *
   * Emitted when a stream is removed from a [class@Valent.MixerAdapter] being
   * monitored by @mixer.
   *
   * Since: 1.0
   */
  signals [STREAM_REMOVED] =
    g_signal_new ("stream-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
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
valent_mixer_init (ValentMixer *self)
{
  self->inputs = g_ptr_array_new_with_free_func (g_object_unref);
  self->outputs = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_mixer_get_default:
 *
 * Get the default [class@Valent.Mixer].
 *
 * Returns: (transfer none) (not nullable): a #ValentMixer
 *
 * Since: 1.0
 */
ValentMixer *
valent_mixer_get_default (void)
{
  if (default_mixer == NULL)
    {
      default_mixer = g_object_new (VALENT_TYPE_MIXER,
                                    "plugin-context",  "mixer",
                                    "plugin-priority", "MixerAdapterPriority",
                                    "plugin-type",     VALENT_TYPE_MIXER_ADAPTER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_mixer),
                                 (gpointer)&default_mixer);
    }

  return default_mixer;
}

/**
 * valent_mixer_get_default_input: (get-property default-input)
 * @mixer: a #ValentMixer
 *
 * Get the default input stream for the primary [class@Valent.MixerAdapter].
 *
 * Returns: (transfer none) (nullable): a #ValentMixerStream
 *
 * Since: 1.0
 */
ValentMixerStream *
valent_mixer_get_default_input (ValentMixer *mixer)
{
  ValentMixerStream *ret = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  if G_LIKELY (mixer->default_adapter != NULL)
    ret = valent_mixer_adapter_get_default_input (mixer->default_adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_set_default_input: (set-property default-input)
 * @mixer: a #ValentMixer
 * @stream: a #ValentMixerStream
 *
 * Set the default input stream for the primary [class@Valent.MixerAdapter].
 *
 * Since: 1.0
 */
void
valent_mixer_set_default_input (ValentMixer       *mixer,
                                ValentMixerStream *stream)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MIXER (mixer));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  if G_LIKELY (mixer->default_adapter != NULL)
    valent_mixer_adapter_set_default_input (mixer->default_adapter, stream);

  VALENT_EXIT;
}

/**
 * valent_mixer_get_default_output: (get-property default-output)
 * @mixer: a #ValentMixer
 *
 * Get the default output stream for the primary [class@Valent.MixerAdapter].
 *
 * Returns: (transfer none) (nullable): a #ValentMixerStream
 *
 * Since: 1.0
 */
ValentMixerStream *
valent_mixer_get_default_output (ValentMixer *mixer)
{
  ValentMixerStream *ret = NULL;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  if G_LIKELY (mixer->default_adapter != NULL)
    ret = valent_mixer_adapter_get_default_output (mixer->default_adapter);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_set_default_output: (set-property default-output)
 * @mixer: a #ValentMixer
 * @stream: a #ValentMixerStream
 *
 * Set the default output stream for the primary [class@Valent.MixerAdapter].
 *
 * Since: 1.0
 */
void
valent_mixer_set_default_output (ValentMixer       *mixer,
                                 ValentMixerStream *stream)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MIXER (mixer));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  if G_LIKELY (mixer->default_adapter != NULL)
    valent_mixer_adapter_set_default_output (mixer->default_adapter, stream);

  VALENT_EXIT;
}

/**
 * valent_mixer_get_inputs:
 * @mixer: a #ValentMixer
 *
 * Get a list of all the input streams being monitored by @mixer.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream): a #GPtrArray
 *
 * Since: 1.0
 */
GPtrArray *
valent_mixer_get_inputs (ValentMixer *mixer)
{
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < mixer->inputs->len; i++)
    g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (mixer->inputs, i)));

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_get_outputs:
 * @mixer: a #ValentMixer
 *
 * Get a list of all the output streams being monitored by @mixer.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream): a #GPtrArray
 *
 * Since: 1.0
 */
GPtrArray *
valent_mixer_get_outputs (ValentMixer *mixer)
{
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < mixer->outputs->len; i++)
    g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (mixer->outputs, i)));

  VALENT_RETURN (ret);
}

