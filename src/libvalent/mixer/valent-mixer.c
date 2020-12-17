// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer"

#include "config.h"

#include <libvalent-core.h>
#include <libpeas/peas.h>

#include "valent-mixer.h"
#include "valent-mixer-control.h"
#include "valent-mixer-stream.h"


/**
 * SECTION:valent-mixer
 * @short_description: Mixer Abstraction
 * @title: ValentMixer
 * @stability: Unstable
 * @include: libvalent-mixer.h
 *
 * #ValentMixer is an aggregator of mixer controls, with a simple API generally intended to be used
 * by #ValentDevicePlugin implementations.
 *
 * Plugins can provide adapters for various backends by subclassing #ValentMixerControl interface.
 */

struct _ValentMixer
{
  ValentComponent     parent_instance;

  ValentMixerControl *default_control;
  GPtrArray          *inputs;
  GPtrArray          *outputs;
};

G_DEFINE_TYPE (ValentMixer, valent_mixer, VALENT_TYPE_COMPONENT)

enum {
  STREAM_ADDED,
  STREAM_CHANGED,
  STREAM_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static ValentMixer *default_mixer = NULL;

/*
 * ValentMixerControl Callbacks
 */
static inline void
valent_mixer_propagate_signal (ValentMixer       *mixer,
                               unsigned int       signal_id,
                               ValentMixerStream *stream)
{
  ValentMixerStreamFlags flags;
  unsigned int detail = 0;

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    detail = valent_mixer_stream_input_quark ();

  else if (flags & VALENT_MIXER_STREAM_SINK)
    detail = valent_mixer_stream_output_quark ();

  g_signal_emit (G_OBJECT (mixer), signal_id, detail, stream);
}

static void
on_stream_added (ValentMixerControl *control,
                 ValentMixerStream  *stream,
                 ValentMixer        *self)
{
  ValentMixerStreamFlags flags;

  g_assert (VALENT_IS_MIXER_CONTROL (control));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    g_ptr_array_add (self->inputs, g_object_ref (stream));

  if (flags & VALENT_MIXER_STREAM_SINK)
    g_ptr_array_add (self->outputs, g_object_ref (stream));

  valent_mixer_propagate_signal (self, signals [STREAM_ADDED], stream);
}

static void
on_stream_changed (ValentMixerControl *control,
                   ValentMixerStream  *stream,
                   ValentMixer        *self)
{
  g_assert (VALENT_IS_MIXER_CONTROL (control));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  valent_mixer_propagate_signal (self, signals [STREAM_CHANGED], stream);
}

static void
on_stream_removed (ValentMixerControl *control,
                   ValentMixerStream  *stream,
                   ValentMixer        *self)
{
  ValentMixerStreamFlags flags;

  g_assert (VALENT_IS_MIXER_CONTROL (control));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  flags = valent_mixer_stream_get_flags (stream);

  if (flags & VALENT_MIXER_STREAM_SOURCE)
    g_ptr_array_remove (self->inputs, stream);

  if (flags & VALENT_MIXER_STREAM_SINK)
    g_ptr_array_remove (self->outputs, stream);

  valent_mixer_propagate_signal (self, signals [STREAM_REMOVED], stream);
}

/*
 * ValentComponent
 */
static void
valent_mixer_provider_added (ValentComponent *component,
                             PeasExtension   *extension)
{
  ValentMixer *self = VALENT_MIXER (component);
  ValentMixerControl *control = VALENT_MIXER_CONTROL (extension);
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  PeasExtension *provider;

  g_assert (VALENT_IS_MIXER (self));
  g_assert (VALENT_IS_MIXER_CONTROL (control));

  /* Add current streams */
  inputs = valent_mixer_control_get_inputs (control);

  for (unsigned int i = 0; i < inputs->len; i++)
    on_stream_added (control, g_ptr_array_index (inputs, i), self);

  outputs = valent_mixer_control_get_outputs (control);

  for (unsigned int i = 0; i < outputs->len; i++)
    on_stream_added (control, g_ptr_array_index (outputs, i), self);

  /* Watch for changes */
  g_signal_connect_object (control,
                           "stream-added",
                           G_CALLBACK (on_stream_added),
                           self, 0);

  g_signal_connect_object (control,
                           "stream-changed",
                           G_CALLBACK (on_stream_changed),
                           self, 0);

  g_signal_connect_object (control,
                           "stream-removed",
                           G_CALLBACK (on_stream_removed),
                           self, 0);

  provider = valent_component_get_priority_provider (component, "MixerControlPriority");

  if ((PeasExtension *)self->default_control != provider)
    g_set_object (&self->default_control, VALENT_MIXER_CONTROL (provider));
}

static void
valent_mixer_provider_removed (ValentComponent *component,
                               PeasExtension   *extension)
{
  ValentMixer *self = VALENT_MIXER (component);
  ValentMixerControl *control = VALENT_MIXER_CONTROL (extension);
  g_autoptr (GPtrArray) inputs = NULL;
  g_autoptr (GPtrArray) outputs = NULL;
  PeasExtension *provider;

  g_assert (VALENT_IS_MIXER (self));
  g_assert (VALENT_IS_MIXER_CONTROL (control));

  /* Simulate stream removal */
  inputs = valent_mixer_control_get_inputs (control);

  for (unsigned int i = 0; i < inputs->len; i++)
    valent_mixer_control_emit_stream_removed (control, g_ptr_array_index (inputs, i));

  outputs = valent_mixer_control_get_outputs (control);

  for (unsigned int i = 0; i < outputs->len; i++)
    valent_mixer_control_emit_stream_removed (control, g_ptr_array_index (outputs, i));

  g_signal_handlers_disconnect_by_data (control, self);

  /* Reset default mixer */
  provider = valent_component_get_priority_provider (component, "MixerControlPriority");
  g_set_object (&self->default_control, VALENT_MIXER_CONTROL (provider));
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
valent_mixer_class_init (ValentMixerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_mixer_finalize;

  component_class->provider_added = valent_mixer_provider_added;
  component_class->provider_removed = valent_mixer_provider_removed;

  /**
   * ValentMixer::stream-added:
   * @control: a #ValentMixer
   * @stream: a #ValentMixerStream
   *
   * The "stream-added" signal is emitted when a new stream is added to the
   * #ValentMixer.
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
   * @control: a #ValentMixer
   * @stream: a #ValentMixerStream
   *
   * The "stream-removed" signal is emitted when a stream is changed from
   * the #ValentMixer.
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
   * @control: a #ValentMixer
   * @stream: a #ValentMixerStream
   *
   * The "stream-removed" signal is emitted when a new stream is removed from
   * the #ValentMixer.
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
 * Get the default #ValentMixer.
 *
 * Returns: (transfer none): The default mixer
 */
ValentMixer *
valent_mixer_get_default (void)
{
  if (default_mixer == NULL)
    {
      default_mixer = g_object_new (VALENT_TYPE_MIXER,
                                    "plugin-context", "mixer",
                                    "plugin-type",    VALENT_TYPE_MIXER_CONTROL,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_mixer),
                                 (gpointer) &default_mixer);
    }

  return default_mixer;
}

/**
 * valent_mixer_get_default_input:
 * @mixer: a #ValentMixer
 *
 * Get the default input stream (eg. microphone).
 *
 * Returns: (transfer none) (nullable): a #ValentMixerStream
 */
ValentMixerStream *
valent_mixer_get_default_input (ValentMixer *mixer)
{
  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  if (mixer->default_control == NULL)
    return NULL;

  return valent_mixer_control_get_default_input (mixer->default_control);
}

/**
 * valent_mixer_get_default_output:
 * @mixer: a #ValentMixer
 *
 * Get the default output stream (eg. speakers).
 *
 * Returns: (transfer none) (nullable): a #ValentMixerStream
 */
ValentMixerStream *
valent_mixer_get_default_output (ValentMixer *mixer)
{
  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  if (mixer->default_control == NULL)
    return NULL;

  return valent_mixer_control_get_default_output (mixer->default_control);
}

/**
 * valent_mixer_get_inputs:
 * @mixer: a #ValentMixer
 *
 * Get a list of all the input streams for @mixer.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream): a #GPtrArray
 */
GPtrArray *
valent_mixer_get_inputs (ValentMixer *mixer)
{
  GPtrArray *streams;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  streams = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < mixer->inputs->len; i++)
    g_ptr_array_add (streams, g_object_ref (g_ptr_array_index (mixer->inputs, i)));

  return streams;
}

/**
 * valent_mixer_get_outputs:
 * @mixer: a #ValentMixer
 *
 * Get a list of all the output streams for @mixer.
 *
 * Returns: (transfer container) (element-type Valent.MixerStream): a #GPtrArray
 */
GPtrArray *
valent_mixer_get_outputs (ValentMixer *mixer)
{
  GPtrArray *streams;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  streams = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < mixer->outputs->len; i++)
    g_ptr_array_add (streams, g_object_ref (g_ptr_array_index (mixer->outputs, i)));

  return streams;
}

