// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer-stream"

#include "config.h"

#include "valent-mixer-enums.h"
#include "valent-mixer-stream.h"


/**
 * SECTION:valent-mixer-stream
 * @short_description: Base class for mixer streams
 * @title: ValentMixerStream
 * @stability: Unstable
 * @include: libvalent-mixer.h
 *
 * #ValentMixerStream is a base class for mixer streams.
 */

typedef struct
{
  char                   *name;
  char                   *description;
  ValentMixerStreamFlags  flags;
  unsigned int            level;
  unsigned int            muted : 1;
} ValentMixerStreamPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentMixerStream, valent_mixer_stream, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DESCRIPTION,
  PROP_FLAGS,
  PROP_LEVEL,
  PROP_MUTED,
  PROP_NAME,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/**
 * ValentMixerStreamClass:
 * @get_name: the virtual function pointer for valent_mixer_stream_get_name()
 * @get_description: the virtual function pointer for valent_mixer_stream_get_description()
 * @get_level: the virtual function pointer for valent_mixer_stream_get_level()
 * @set_level: the virtual function pointer for valent_mixer_stream_set_level()
 * @get_muted: the virtual function pointer for valent_mixer_stream_get_muted()
 * @set_muted: the virtual function pointer for valent_mixer_stream_set_muted()
 *
 * The virtual function table for #ValentMixerStream.
 */

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

  switch (prop_id)
    {
    case PROP_DESCRIPTION:
      g_value_set_string (value, valent_mixer_stream_get_description (self));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, valent_mixer_stream_get_flags (self));
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

  switch (prop_id)
    {
    case PROP_DESCRIPTION:
      priv->description = g_value_dup_string (value);
      break;

    case PROP_FLAGS:
      priv->flags = g_value_get_flags (value);
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
  stream_class->get_level = valent_mixer_stream_real_get_level;
  stream_class->set_level = valent_mixer_stream_real_set_level;
  stream_class->get_muted = valent_mixer_stream_real_get_muted;
  stream_class->set_muted = valent_mixer_stream_real_set_muted;

  /**
   * ValentMixerStream:description:
   *
   * The "description" property holds the human-readable label of the stream.
   */
  properties [PROP_DESCRIPTION] =
    g_param_spec_string ("description",
                         "Description",
                         "The human-readable label of the stream",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:flags:
   *
   * The "falgs" property holds the input or output level of the stream.
   */
  properties [PROP_FLAGS] =
    g_param_spec_flags ("flags",
                        "Flags",
                        "The type flags for the stream",
                        VALENT_TYPE_MIXER_STREAM_FLAGS,
                        VALENT_MIXER_STREAM_UNKNOWN,
                        (G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:level:
   *
   * The "level" property holds the input or output level of the stream.
   */
  properties [PROP_LEVEL] =
    g_param_spec_uint ("level",
                       "Level",
                       "The input or output level of the stream",
                       0, 100,
                       0,
                       (G_PARAM_READWRITE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:muted:
   *
   * The "muted" property indicates whether the stream is muted.
   */
  properties [PROP_MUTED] =
    g_param_spec_boolean ("muted",
                          "Muted",
                          "Whether the stream is muted",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMixerStream:name:
   *
   * The "name" property holds the unique name of the stream.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The unique name of the stream",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_mixer_stream_init (ValentMixerStream *stream)
{
}

/**
 * valent_mixer_stream_get_name:
 * @stream: a #ValentMixerStream
 *
 * Get the unique stream name. For strings to display to users, see
 * valent_mixer_stream_dup_label(), valent_mixer_stream_get_primary_text() and
 * valent_mixer_stream_get_secondary_text().
 *
 * Returns: (transfer none): a string name
 */
const char *
valent_mixer_stream_get_name (ValentMixerStream *stream)
{
  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), NULL);

  return VALENT_MIXER_STREAM_GET_CLASS (stream)->get_name (stream);
}

/**
 * valent_mixer_stream_get_description:
 * @stream: a #ValentMixerStream
 *
 * Get the human-readable label for @stream.
 *
 * Returns: (transfer none): a newly allocated string
 */
const char *
valent_mixer_stream_get_description (ValentMixerStream *stream)
{
  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), NULL);

  return VALENT_MIXER_STREAM_GET_CLASS (stream)->get_description (stream);
}

/**
 * valent_mixer_stream_get_flags:
 * @stream: a #ValentMixerStream
 *
 * Get the type flags for the stream.
 *
 * Returns: the #ValentMixerStreamFlags that apply to @stream
 */
ValentMixerStreamFlags
valent_mixer_stream_get_flags (ValentMixerStream *stream)
{
  ValentMixerStreamPrivate *priv = valent_mixer_stream_get_instance_private (stream);

  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), VALENT_MIXER_STREAM_UNKNOWN);

  return priv->flags;
}

/**
 * valent_mixer_stream_get_level:
 * @stream: a #ValentMixerStream
 *
 * Get the level of the stream (eg. speaker volume, microphone sensitivity).
 *
 * Returns: stream level
 */
unsigned int
valent_mixer_stream_get_level (ValentMixerStream *stream)
{
  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), 0);

  return VALENT_MIXER_STREAM_GET_CLASS (stream)->get_level (stream);
}

/**
 * valent_mixer_stream_set_level:
 * @stream: a #ValentMixerStream
 * @level: a level
 *
 * Set the level of the stream (eg. speaker volume, microphone sensitivity).
 */
void
valent_mixer_stream_set_level (ValentMixerStream *stream,
                               unsigned int       level)
{
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  VALENT_MIXER_STREAM_GET_CLASS (stream)->set_level (stream, level);
}

/**
 * valent_mixer_stream_get_muted:
 * @stream: a #ValentMixerStream
 *
 * Get the muted state of the default input stream (eg. microphone on/off).
 *
 * Returns: mute state
 */
gboolean
valent_mixer_stream_get_muted (ValentMixerStream *stream)
{
  g_return_val_if_fail (VALENT_IS_MIXER_STREAM (stream), FALSE);

  return VALENT_MIXER_STREAM_GET_CLASS (stream)->get_muted (stream);
}

/**
 * valent_mixer_stream_set_muted:
 * @stream: a #ValentMixerStream
 * @state: a muted
 *
 * Set the muted state of the default input stream (eg. microphone on/off).
 */
void
valent_mixer_stream_set_muted (ValentMixerStream *stream,
                               gboolean           state)
{
  g_return_if_fail (VALENT_IS_MIXER_STREAM (stream));

  VALENT_MIXER_STREAM_GET_CLASS (stream)->set_muted (stream, state);
}

