// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer-stream"

#include "config.h"

#include <libvalent-core.h>

#include "valent-mixer-enums.h"
#include "valent-mixer-stream.h"


/**
 * ValentMixerStream:
 *
 * A base class for mixer streams.
 *
 * `ValentMixerStream` is a base class for mixer streams, intended for use by
 * implementations of [class@Valent.MixerAdapter].
 *
 * Since: 1.0
 */

typedef struct
{
  char                 *name;
  char                 *description;
  ValentMixerDirection  direction;
  unsigned int          level;
  unsigned int          muted : 1;
} ValentMixerStreamPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentMixerStream, valent_mixer_stream, VALENT_TYPE_RESOURCE)

typedef enum {
  PROP_DESCRIPTION = 1,
  PROP_DIRECTION,
  PROP_LEVEL,
  PROP_MUTED,
  PROP_NAME,
} ValentMixerStreamProperty;

static GParamSpec *properties[PROP_NAME + 1] = { NULL, };

/* LCOV_EXCL_START */
static const char *
valent_mixer_stream_real_get_name (ValentMixerStream *stream)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  if (priv->name == NULL)
    priv->name = g_uuid_string_random ();

  return priv->name;
}

static const char *
valent_mixer_stream_real_get_description (ValentMixerStream *stream)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  if (priv->description == NULL)
    return "Unnamed Stream";

  return priv->description;
}

ValentMixerDirection
valent_mixer_stream_real_get_direction (ValentMixerStream *stream)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  return priv->direction;
}

static unsigned int
valent_mixer_stream_real_get_level (ValentMixerStream *stream)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  return priv->level;
}

static void
valent_mixer_stream_real_set_level (ValentMixerStream *stream,
                                    unsigned int       level)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  if (priv->level == level)
    return;

  priv->level = level;
  g_object_notify_by_pspec (G_OBJECT (stream), properties [PROP_LEVEL]);
}

static gboolean
valent_mixer_stream_real_get_muted (ValentMixerStream *stream)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  return priv->muted;
}

static void
valent_mixer_stream_real_set_muted (ValentMixerStream *stream,
                                    gboolean           mute)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  if (priv->muted == mute)
    return;

  priv->muted = mute;
  g_object_notify_by_pspec (G_OBJECT (stream), properties [PROP_MUTED]);
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_mixer_stream_finalize (GObject *object)
{
  ValentMixerStream *self = VALENT_MIXER_STREAM (object);
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (self);

  g_clear_pointer (&priv->name, g_free);
  g_clear_pointer (&priv->description, g_free);

  G_OBJECT_CLASS (valent_mixer_stream_parent_class)->finalize (object);
}

static void
valent_mixer_stream_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentMixerStream *self = VALENT_MIXER_STREAM (object);

  switch ((ValentMixerStreamProperty)prop_id)
    {
    case PROP_DESCRIPTION:
      g_value_set_string (value, valent_mixer_stream_get_description (self));
      break;

    case PROP_DIRECTION:
      g_value_set_enum (value, valent_mixer_stream_get_direction (self));
      break;

    case PROP_LEVEL:
      g_value_set_uint (value, valent_mixer_stream_get_level (self));
      break;

    case PROP_MUTED:
      g_value_set_boolean (value, valent_mixer_stream_get_muted (self));
      break;

    case PROP_NAME:
      g_value_set_string (value, valent_mixer_stream_get_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_stream_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentMixerStream *self = VALENT_MIXER_STREAM (object);
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (self);

  switch ((ValentMixerStreamProperty)prop_id)
    {
    case PROP_DESCRIPTION:
      priv->description = g_value_dup_string (value);
      break;

    case PROP_DIRECTION:
      priv->direction = g_value_get_enum (value);
      break;

    case PROP_LEVEL:
      valent_mixer_stream_set_level (self, g_value_get_uint (value));
      break;

    case PROP_MUTED:
      valent_mixer_stream_set_muted (self, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_stream_class_init (ValentMixerStreamClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentMixerStreamClass *stream_class = VALENT_MIXER_STREAM_CLASS (klass);

  object_class->finalize = valent_mixer_stream_finalize;
  object_class->get_property = valent_mixer_stream_get_property;
  object_class->set_property = valent_mixer_stream_set_property;

  stream_class->get_name = valent_mixer_stream_real_get_name;
  stream_class->get_description = valent_mixer_stream_real_get_description;
  stream_class->get_direction = valent_mixer_stream_real_get_direction;
  stream_class->get_level = valent_mixer_stream_real_get_level;
  stream_class->set_level = valent_mixer_stream_real_set_level;
  stream_class->get_muted = valent_mixer_stream_real_get_muted;
  stream_class->set_muted = valent_mixer_stream_real_set_muted;

  /**
   * ValentMixerStream:description: (getter get_description)
   *
   * The human-readable label of the stream.
   *
   * Implementation may emit [signal@GObject.Object::notify] for this property
   * if it changes.
   *
   * Since: 1.0
   */
  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:direction: (getter get_direction)
   *
   * The port direction of the stream.
   *
   * Since: 1.0
   */
  properties [PROP_DIRECTION] =
    g_param_spec_enum ("direction", NULL, NULL,
                       VALENT_TYPE_MIXER_DIRECTION,
                       VALENT_MIXER_INPUT,
                       (G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:level: (getter get_level) (setter set_level)
   *
   * The input or output level of the stream.
   *
   * Since: 1.0
   */
  properties [PROP_LEVEL] =
    g_param_spec_uint ("level", NULL, NULL,
                       0, 100,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:muted: (getter get_muted) (setter set_muted)
   *
   * Whether the stream is muted.
   *
   * Since: 1.0
   */
  properties [PROP_MUTED] =
    g_param_spec_boolean ("muted", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:name: (getter get_name)
   *
   * The unique name of the stream.
   *
   * Since: 1.0
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_mixer_stream_init (ValentMixerStream *stream)
{
}

/**
 * valent_mixer_stream_get_name: (get-property name) (virtual get_name)
 * @stream: a `ValentMixerStream`
 *
 * Get the unique name of @stream.
 *
 * Returns: (transfer none): a unique name
 *
 * Since: 1.0
 */
const char *
valent_mixer_stream_get_name (ValentMixerStream *stream)
{
  const char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), NULL);

  ret = VALENT_MIXER_STREAM_GET_CLASS (stream)->get_name (stream);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_stream_get_description: (get-property description) (virtual get_description)
 * @stream: a `ValentMixerStream`
 *
 * Get the human-readable label of @stream.
 *
 * Returns: (transfer none): a stream description
 *
 * Since: 1.0
 */
const char *
valent_mixer_stream_get_description (ValentMixerStream *stream)
{
  const char *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), NULL);

  ret = VALENT_MIXER_STREAM_GET_CLASS (stream)->get_description (stream);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_stream_get_direction: (get-property direction) (virtual get_direction)
 * @stream: a `ValentMixerStream`
 *
 * Get the port direction of @stream.
 *
 * Returns: the `ValentMixerDirection` of @stream
 *
 * Since: 1.0
 */
ValentMixerDirection
valent_mixer_stream_get_direction (ValentMixerStream *stream)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), VALENT_MIXER_INPUT);

  return priv->direction;
}

/**
 * valent_mixer_stream_get_level: (get-property level) (virtual get_level)
 * @stream: a `ValentMixerStream`
 *
 * Get the level of @stream (eg. speaker volume, microphone sensitivity).
 *
 * Implementations that override this method should also override
 * [vfunc@Valent.MixerStream.set_level].
 *
 * Returns: a volume level between `0` and `100`
 *
 * Since: 1.0
 */
unsigned int
valent_mixer_stream_get_level (ValentMixerStream *stream)
{
  unsigned int ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), 0);

  ret = VALENT_MIXER_STREAM_GET_CLASS (stream)->get_level (stream);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_stream_set_level: (set-property level) (virtual set_level)
 * @stream: a `ValentMixerStream`
 * @level: a volume level between `0` and `100`
 *
 * Set the level of @stream (eg. speaker volume, microphone sensitivity).
 *
 * Implementations that override this method should also override
 * [vfunc@Valent.MixerStream.get_level].
 *
 * Since: 1.0
 */
void
valent_mixer_stream_set_level (ValentMixerStream *stream,
                               unsigned int       level)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));
  g_return_if_fail (level <= 100);

  VALENT_MIXER_STREAM_GET_CLASS (stream)->set_level (stream, level);

  VALENT_EXIT;
}

/**
 * valent_mixer_stream_get_muted: (get-property muted) (virtual get_muted)
 * @stream: a `ValentMixerStream`
 *
 * Get the muted state of @stream.
 *
 * Implementations that override this method should also override
 * [vfunc@Valent.MixerStream.set_muted].
 *
 * Returns: %TRUE if the stream is muted, or %FALSE if not
 *
 * Since: 1.0
 */
gboolean
valent_mixer_stream_get_muted (ValentMixerStream *stream)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), FALSE);

  ret = VALENT_MIXER_STREAM_GET_CLASS (stream)->get_muted (stream);

  VALENT_RETURN (ret);
}

/**
 * valent_mixer_stream_set_muted: (set-property muted) (virtual set_muted)
 * @stream: a `ValentMixerStream`
 * @state: whether the stream should be muted
 *
 * Set the muted state of @stream.
 *
 * Implementations that override this method should also override
 * [vfunc@Valent.MixerStream.get_muted].
 *
 * Since: 1.0
 */
void
valent_mixer_stream_set_muted (ValentMixerStream *stream,
                               gboolean           state)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  VALENT_MIXER_STREAM_GET_CLASS (stream)->set_muted (stream, state);

  VALENT_EXIT;
}

