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


/*< private >
 *
 * ValentMixerStreams:
 *
 * A [iface@Gio.ListModel] implementation for grouping inputs and outputs.
 */
#define VALENT_TYPE_MIXER_STREAMS (valent_mixer_streams_get_type ())

G_DECLARE_FINAL_TYPE (ValentMixerStreams, valent_mixer_streams, VALENT, MIXER_STREAMS, GObject)

struct _ValentMixerStreams
{
  GObject    parent_instance;
  GPtrArray *items;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMixerStreams, valent_mixer_streams, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))


static gpointer
valent_mixer_streams_get_item (GListModel   *list,
                               unsigned int  position)
{
  ValentMixerStreams *self = VALENT_MIXER_STREAMS (list);

  g_assert (VALENT_IS_MIXER_STREAMS (self));
  g_assert (position < self->items->len);

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
valent_mixer_streams_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MIXER_STREAM;
}

static unsigned int
valent_mixer_streams_get_n_items (GListModel *list)
{
  ValentMixerStreams *self = VALENT_MIXER_STREAMS (list);

  g_assert (VALENT_IS_MIXER_STREAMS (self));

  return self->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_mixer_streams_get_item;
  iface->get_item_type = valent_mixer_streams_get_item_type;
  iface->get_n_items = valent_mixer_streams_get_n_items;
}

static void
valent_mixer_streams_finalize (GObject *object)
{
  ValentMixerStreams *self = VALENT_MIXER_STREAMS (object);

  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_mixer_streams_parent_class)->finalize (object);
}

static void
valent_mixer_streams_class_init (ValentMixerStreamsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_mixer_streams_finalize;
}

static void
valent_mixer_streams_init (ValentMixerStreams *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
}

static inline void
valent_mixer_streams_add (ValentMixerStreams *self,
                          ValentMixerStream  *stream)
{
  unsigned int position = 0;

  g_assert (VALENT_IS_MIXER_STREAMS (self));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  position = self->items->len;
  g_ptr_array_add (self->items, g_object_ref (stream));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);
}

#if 0
static inline void
valent_mixer_streams_insert (ValentMixerStreams *self,
                             unsigned int        position,
                             ValentMixerStream  *stream)
{
  g_assert (VALENT_IS_MIXER_STREAMS (self));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  g_ptr_array_insert (self->items, position, g_object_ref (stream));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}
#endif

static inline void
valent_mixer_streams_remove (ValentMixerStreams *self,
                             ValentMixerStream  *stream)
{
  unsigned int position = 0;

  g_assert (VALENT_IS_MIXER_STREAMS (self));
  g_assert (VALENT_IS_MIXER_STREAM (stream));

  if (g_ptr_array_find (self->items, stream, &position))
    {
      g_ptr_array_remove_index (self->items, position);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    }
}


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
  GListModel         *inputs;
  GListModel         *outputs;
  GPtrArray          *streams;
};

G_DEFINE_TYPE (ValentMixer, valent_mixer, VALENT_TYPE_COMPONENT)

enum {
  PROP_0,
  PROP_DEFAULT_INPUT,
  PROP_DEFAULT_OUTPUT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static ValentMixer *default_mixer = NULL;


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

static void
on_stream_added (ValentMixerAdapter *adapter,
                 ValentMixerStream  *stream,
                 ValentMixer        *self)
{
  ValentMixerDirection direction = VALENT_MIXER_INPUT;

  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  direction = valent_mixer_stream_get_direction (stream);

  if (direction == VALENT_MIXER_INPUT && self->inputs != NULL)
    valent_mixer_streams_add (VALENT_MIXER_STREAMS (self->inputs), stream);
  else if (direction == VALENT_MIXER_OUTPUT && self->outputs != NULL)
    valent_mixer_streams_add (VALENT_MIXER_STREAMS (self->outputs), stream);

  g_ptr_array_add (self->streams, g_object_ref (stream));
}

static void
on_stream_removed (ValentMixerAdapter *adapter,
                   ValentMixerStream  *stream,
                   ValentMixer        *self)
{
  ValentMixerDirection direction = VALENT_MIXER_INPUT;

  g_assert (VALENT_IS_MIXER_ADAPTER (adapter));
  g_assert (VALENT_IS_MIXER_STREAM (stream));
  g_assert (VALENT_IS_MIXER (self));

  direction = valent_mixer_stream_get_direction (stream);

  if (direction == VALENT_MIXER_INPUT && self->inputs != NULL)
    valent_mixer_streams_remove (VALENT_MIXER_STREAMS (self->inputs), stream);
  else if (direction == VALENT_MIXER_OUTPUT && self->outputs != NULL)
    valent_mixer_streams_remove (VALENT_MIXER_STREAMS (self->outputs), stream);

  g_ptr_array_remove (self->streams, stream);
}

/*
 * ValentComponent
 */
static void
valent_mixer_bind_extension (ValentComponent *component,
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
                           "stream-removed",
                           G_CALLBACK (on_stream_removed),
                           self, 0);

  /* Set default provider */
  new_primary = valent_component_get_primary (component);
  self->default_adapter = VALENT_MIXER_ADAPTER (new_primary);

  VALENT_EXIT;
}

static void
valent_mixer_unbind_extension (ValentComponent *component,
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
    valent_mixer_adapter_stream_removed (adapter, g_ptr_array_index (inputs, i));

  outputs = valent_mixer_adapter_get_outputs (adapter);

  for (unsigned int i = 0; i < outputs->len; i++)
    valent_mixer_adapter_stream_removed (adapter, g_ptr_array_index (outputs, i));

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

  g_clear_weak_pointer (&self->inputs);
  g_clear_weak_pointer (&self->outputs);
  g_clear_pointer (&self->streams, g_ptr_array_unref);

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

  component_class->bind_extension = valent_mixer_bind_extension;
  component_class->unbind_extension = valent_mixer_unbind_extension;

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
}

static void
valent_mixer_init (ValentMixer *self)
{
  self->streams = g_ptr_array_new_with_free_func (g_object_unref);
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
 * Returns: (transfer full): a #GListModel
 *
 * Since: 1.0
 */
GListModel *
valent_mixer_get_inputs (ValentMixer *mixer)
{
  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  if (mixer->inputs != NULL)
    VALENT_RETURN (g_object_ref (mixer->inputs));

  mixer->inputs = g_object_new (VALENT_TYPE_MIXER_STREAMS, NULL);
  g_object_add_weak_pointer (G_OBJECT (mixer->inputs),
                             (gpointer *)&mixer->inputs);

  for (unsigned int i = 0; i < mixer->streams->len; i++)
    {
      ValentMixerStream *stream = g_ptr_array_index (mixer->streams, i);

      if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_INPUT)
        valent_mixer_streams_add (VALENT_MIXER_STREAMS (mixer->inputs), stream);
    }

  VALENT_RETURN (mixer->inputs);
}

/**
 * valent_mixer_get_outputs:
 * @mixer: a #ValentMixer
 *
 * Get a list of all the output streams being monitored by @mixer.
 *
 * Returns: (transfer full): a #GListModel
 *
 * Since: 1.0
 */
GListModel *
valent_mixer_get_outputs (ValentMixer *mixer)
{
  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER (mixer), NULL);

  if (mixer->outputs != NULL)
    VALENT_RETURN (g_object_ref (mixer->outputs));

  mixer->outputs = g_object_new (VALENT_TYPE_MIXER_STREAMS, NULL);
  g_object_add_weak_pointer (G_OBJECT (mixer->outputs),
                             (gpointer *)&mixer->outputs);

  for (unsigned int i = 0; i < mixer->streams->len; i++)
    {
      ValentMixerStream *stream = g_ptr_array_index (mixer->streams, i);

      if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_OUTPUT)
        valent_mixer_streams_add (VALENT_MIXER_STREAMS (mixer->outputs), stream);
    }

  VALENT_RETURN (mixer->outputs);
}

