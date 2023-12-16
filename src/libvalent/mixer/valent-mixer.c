// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-mixer.h"
#include "valent-mixer-adapter.h"
#include "valent-mixer-stream.h"


/**
 * ValentMixer:
 *
 * A class for monitoring and controlling the system volume.
 *
 * `ValentMixer` is an abstraction of volume mixers, intended for use by
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
  GPtrArray          *streams;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMixer, valent_mixer, VALENT_TYPE_COMPONENT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

enum {
  PROP_0,
  PROP_DEFAULT_INPUT,
  PROP_DEFAULT_OUTPUT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static ValentMixer *default_mixer = NULL;

#if 0
  /* Translate the position */
  for (unsigned int i = 0; i < self->adapters->len; i++)
    {
      GListModel *adapter = g_ptr_array_index (self->adapters, i);

      if (adapter == list)
        break;

      offset += g_list_model_get_n_items (adapter);
    }
#endif


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
on_items_changed (GListModel   *list,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  ValentMixer  *self)
{
  g_assert (VALENT_IS_MIXER_ADAPTER (list));
  g_assert (VALENT_IS_MIXER (self));

  /* Remove items */
  while (removed--)
    g_ptr_array_remove_index (self->streams, position);

  /* Add items */
  for (unsigned int i = 0; i < added; i++)
    {
      g_autoptr (ValentMixerStream) stream = NULL;

      stream = g_list_model_get_item (list, position + i);
      g_ptr_array_insert (self->streams, position + i, g_steal_pointer (&stream));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

/*
 * ValentComponent
 */
static void
valent_mixer_bind_preferred (ValentComponent *component,
                             GObject         *extension)
{
  ValentMixer *self = VALENT_MIXER (component);
  ValentMixerAdapter *adapter = VALENT_MIXER_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_MIXER (self));
  g_assert (adapter == NULL || VALENT_IS_MIXER_ADAPTER (adapter));

  if (self->default_adapter != NULL)
    {
      GListModel *list = G_LIST_MODEL (self->default_adapter);

      g_signal_handlers_disconnect_by_func (self->default_adapter,
                                            self,
                                            on_items_changed);
      g_signal_handlers_disconnect_by_func (self->default_adapter,
                                            self,
                                            on_default_input_changed);
      g_signal_handlers_disconnect_by_func (self->default_adapter,
                                            self,
                                            on_default_output_changed);
      on_items_changed (list, 0, g_list_model_get_n_items (list), 0, self);
      self->default_adapter = NULL;
    }

  if (adapter != NULL)
    {
      GListModel *list = G_LIST_MODEL (adapter);

      g_signal_connect_object (adapter,
                               "notify::default-input",
                               G_CALLBACK (on_default_input_changed),
                               self, 0);
      g_signal_connect_object (adapter,
                               "notify::default-output",
                               G_CALLBACK (on_default_output_changed),
                               self, 0);
      g_signal_connect_object (adapter,
                               "items-changed",
                               G_CALLBACK (on_items_changed),
                               self, 0);
      on_items_changed (list, 0, 0, g_list_model_get_n_items (list), self);
      g_object_notify (G_OBJECT (self), "default-input");
      g_object_notify (G_OBJECT (self), "default-output");
      self->default_adapter = adapter;
    }

  VALENT_EXIT;
}

/*
 * GListModel
 */
static gpointer
valent_mixer_get_item (GListModel   *list,
                       unsigned int  position)
{
  ValentMixer *self = VALENT_MIXER (list);

  g_assert (VALENT_IS_MIXER (self));

  if G_UNLIKELY (position >= self->streams->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->streams, position));
}

static GType
valent_mixer_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MIXER_STREAM;
}

static unsigned int
valent_mixer_get_n_items (GListModel *list)
{
  ValentMixer *self = VALENT_MIXER (list);

  g_assert (VALENT_IS_MIXER (self));

  return self->streams->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_mixer_get_item;
  iface->get_item_type = valent_mixer_get_item_type;
  iface->get_n_items = valent_mixer_get_n_items;
}

/*
 * GObject
 */
static void
valent_mixer_finalize (GObject *object)
{
  ValentMixer *self = VALENT_MIXER (object);

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

  component_class->bind_preferred = valent_mixer_bind_preferred;

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
 * Returns: (transfer none) (not nullable): a `ValentMixer`
 *
 * Since: 1.0
 */
ValentMixer *
valent_mixer_get_default (void)
{
  if (default_mixer == NULL)
    {
      default_mixer = g_object_new (VALENT_TYPE_MIXER,
                                    "plugin-domain", "mixer",
                                    "plugin-type",   VALENT_TYPE_MIXER_ADAPTER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_mixer),
                                 (gpointer)&default_mixer);
    }

  return default_mixer;
}

/**
 * valent_mixer_get_default_input: (get-property default-input)
 * @mixer: a `ValentMixer`
 *
 * Get the default input stream for the primary [class@Valent.MixerAdapter].
 *
 * Returns: (transfer none) (nullable): a `ValentMixerStream`
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
 * @mixer: a `ValentMixer`
 * @stream: a `ValentMixerStream`
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
 * @mixer: a `ValentMixer`
 *
 * Get the default output stream for the primary [class@Valent.MixerAdapter].
 *
 * Returns: (transfer none) (nullable): a `ValentMixerStream`
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
 * @mixer: a `ValentMixer`
 * @stream: a `ValentMixerStream`
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

