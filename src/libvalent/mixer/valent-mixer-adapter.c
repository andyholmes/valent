// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer-adapter"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

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
  GPtrArray *streams;
} ValentMixerAdapterPrivate;

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentMixerAdapter, valent_mixer_adapter, VALENT_TYPE_EXTENSION,
                                  G_ADD_PRIVATE (ValentMixerAdapter)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))


/**
 * ValentMixerAdapterClass:
 * @get_default_input: the virtual function pointer for valent_mixer_adapter_get_default_input()
 * @set_default_input: the virtual function pointer for valent_mixer_adapter_set_default_input()
 * @get_default_output: the virtual function pointer for valent_mixer_adapter_get_default_output()
 * @set_default_output: the virtual function pointer for valent_mixer_adapter_set_default_output()
 *
 * The virtual function table for #ValentMixerAdapter.
 */

enum {
  PROP_0,
  PROP_DEFAULT_INPUT,
  PROP_DEFAULT_OUTPUT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GListModel
 */
static gpointer
valent_mixer_adapter_get_item (GListModel   *list,
                               unsigned int  position)
{
  ValentMixerAdapter *self = VALENT_MIXER_ADAPTER (list);
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  g_assert (VALENT_IS_MIXER_ADAPTER (self));

  if G_UNLIKELY (position >= priv->streams->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (priv->streams, position));
}

static GType
valent_mixer_adapter_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MIXER_STREAM;
}

static unsigned int
valent_mixer_adapter_get_n_items (GListModel *list)
{
  ValentMixerAdapter *self = VALENT_MIXER_ADAPTER (list);
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  g_assert (VALENT_IS_MIXER_ADAPTER (self));

  return priv->streams->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_mixer_adapter_get_item;
  iface->get_item_type = valent_mixer_adapter_get_item_type;
  iface->get_n_items = valent_mixer_adapter_get_n_items;
}

/* LCOV_EXCL_START */
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
 * ValentObject
 */
static void
valent_mixer_adapter_destroy (ValentObject *object)
{
  ValentMixerAdapter *self = VALENT_MIXER_ADAPTER (object);
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  g_clear_pointer (&priv->streams, g_ptr_array_unref);

  VALENT_OBJECT_CLASS (valent_mixer_adapter_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_mixer_adapter_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentMixerAdapter *self = VALENT_MIXER_ADAPTER (object);

  switch (prop_id)
    {
    case PROP_DEFAULT_INPUT:
      g_value_set_object (value, valent_mixer_adapter_get_default_input (self));
      break;

    case PROP_DEFAULT_OUTPUT:
      g_value_set_object (value, valent_mixer_adapter_get_default_output (self));
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

  switch (prop_id)
    {
    case PROP_DEFAULT_INPUT:
      valent_mixer_adapter_set_default_input (self, g_value_get_object (value));
      break;

    case PROP_DEFAULT_OUTPUT:
      valent_mixer_adapter_set_default_output (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_adapter_class_init (ValentMixerAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);

  object_class->get_property = valent_mixer_adapter_get_property;
  object_class->set_property = valent_mixer_adapter_set_property;

  vobject_class->destroy = valent_mixer_adapter_destroy;

  klass->get_default_input = valent_mixer_adapter_real_get_default_input;
  klass->set_default_input = valent_mixer_adapter_real_set_default_input;
  klass->get_default_output = valent_mixer_adapter_real_get_default_output;
  klass->set_default_output = valent_mixer_adapter_real_set_default_output;

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

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mixer_adapter_init (ValentMixerAdapter *self)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (self);

  priv->streams = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_mixer_adapter_stream_added:
 * @adapter: a #ValentMixerAdapter
 * @stream: a #ValentMixerStream
 *
 * Called when @stream has been added to the mixer.
 *
 * This method should only be called by implementations of
 * [class@Valent.MixerAdapter]. @adapter will hold a reference on @stream and
 * emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_mixer_adapter_stream_added (ValentMixerAdapter *adapter,
                                   ValentMixerStream  *stream)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (adapter);
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_MIXER_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  position = priv->streams->len;
  g_ptr_array_add (priv->streams, g_object_ref (stream));
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 0, 1);
}

/**
 * valent_mixer_adapter_stream_removed:
 * @adapter: a #ValentMixerAdapter
 * @stream: a #ValentMixerStream
 *
 * Called when @stream has been removed from the mixer.
 *
 * This method should only be called by implementations of
 * [class@Valent.MixerAdapter]. @adapter will drop its reference on @stream and
 * emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_mixer_adapter_stream_removed (ValentMixerAdapter *adapter,
                                     ValentMixerStream  *stream)
{
  ValentMixerAdapterPrivate *priv = valent_mixer_adapter_get_instance_private (adapter);
  g_autoptr (ValentMixerStream) item = NULL;
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_MIXER_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  if (!g_ptr_array_find (priv->streams, stream, &position))
    return;

  item = g_ptr_array_steal_index (priv->streams, position);
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 1, 0);
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

