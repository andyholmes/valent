// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-message"

#include "config.h"

#include <gio/gio.h>
#include <valent.h>

#include "valent-message.h"


struct _ValentMessage
{
  GObject           parent_instance;

  ValentMessageBox  box;
  gint64            date;
  gint64            id;
  GVariant         *metadata;
  unsigned int      read : 1;
  char             *sender;
  char             *text;
  gint64            thread_id;
};

G_DEFINE_FINAL_TYPE (ValentMessage, valent_message, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_BOX,
  PROP_DATE,
  PROP_ID,
  PROP_METADATA,
  PROP_READ,
  PROP_SENDER,
  PROP_TEXT,
  PROP_THREAD_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_message_finalize (GObject *object)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  g_clear_pointer (&self->sender, g_free);
  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->metadata, g_variant_unref);

  G_OBJECT_CLASS (valent_message_parent_class)->finalize (object);
}

static void
valent_message_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_BOX:
      g_value_set_uint (value, self->box);
      break;

    case PROP_DATE:
      g_value_set_int64 (value, self->date);
      break;

    case PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case PROP_METADATA:
      g_value_set_variant (value, valent_message_get_metadata (self));
      break;

    case PROP_READ:
      g_value_set_boolean (value, self->read);
      break;

    case PROP_SENDER:
      g_value_set_string (value, self->sender);
      break;

    case PROP_TEXT:
      g_value_set_string (value, self->text);
      break;

    case PROP_THREAD_ID:
      g_value_set_int64 (value, self->thread_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_BOX:
      self->box = g_value_get_uint (value);
      break;

    case PROP_DATE:
      self->date = g_value_get_int64 (value);
      break;

    case PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case PROP_METADATA:
      self->metadata = g_value_dup_variant (value);
      break;

    case PROP_READ:
      valent_message_set_read (self, g_value_get_boolean (value));
      break;

    case PROP_SENDER:
      self->sender = g_value_dup_string (value);
      break;

    case PROP_TEXT:
      self->text = g_value_dup_string (value);
      break;

    case PROP_THREAD_ID:
      self->thread_id = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_class_init (ValentMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_message_finalize;
  object_class->get_property = valent_message_get_property;
  object_class->set_property = valent_message_set_property;

  /**
   * ValentMessage:box:
   *
   * The #ValentMessageBox of the message.
   */
  properties [PROP_BOX] =
    g_param_spec_uint ("box", NULL, NULL,
                       VALENT_MESSAGE_BOX_ALL, VALENT_MESSAGE_BOX_FAILED,
                       VALENT_MESSAGE_BOX_ALL,
                       (G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:date:
   *
   * A UNIX epoch timestamp for the message.
   */
  properties [PROP_DATE] =
    g_param_spec_int64 ("date", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:id:
   *
   * The unique ID for this message.
   */
  properties [PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:metadata:
   *
   * Ancillary data for the message, such as media.
   */
  properties [PROP_METADATA] =
    g_param_spec_variant ("metadata", NULL, NULL,
                          G_VARIANT_TYPE_VARDICT,
                          NULL,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:read:
   *
   * Whether the message has been read.
   */
  properties [PROP_READ] =
    g_param_spec_boolean ("read", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:sender:
   *
   * The sender of the message. This will usually be a phone number or other
   * address form.
   */
  properties [PROP_SENDER] =
    g_param_spec_string ("sender", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:text:
   *
   * The text content of the message.
   */
  properties [PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:thread-id:
   *
   * The thread this message belongs to.
   */
  properties [PROP_THREAD_ID] =
    g_param_spec_int64 ("thread-id", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_message_init (ValentMessage *message)
{
}

/**
 * valent_message_get_box:
 * @message: a #ValentMessage
 *
 * Get the #ValentMessageBox of @message.
 *
 * Returns: a #ValentMessageBox
 */
ValentMessageBox
valent_message_get_box (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), VALENT_MESSAGE_BOX_ALL);

  return message->box;
}

/**
 * valent_message_get_date:
 * @message: a #ValentMessage
 *
 * Get the timestamp for @message.
 *
 * Returns: the message timestamp
 */
gint64
valent_message_get_date (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->date;
}

/**
 * valent_message_get_id:
 * @message: a #ValentMessage
 *
 * Get the unique ID for @message.
 *
 * Returns: the message ID
 */
gint64
valent_message_get_id (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->id;
}

/**
 * valent_message_get_metadata:
 * @message: a #ValentMessage
 *
 * Get the #GVariant dictionary of metadata.
 *
 * Returns: (transfer none) (nullable): the metadata
 */
GVariant *
valent_message_get_metadata (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->metadata;
}

/**
 * valent_message_get_read:
 * @message: a #ValentMessage
 *
 * Get the read status of @message.
 *
 * Returns: %TRUE if the message has been read
 */
gboolean
valent_message_get_read (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), FALSE);

  return message->read;
}

/**
 * valent_message_set_read:
 * @message: a #ValentMessage
 * @read: whether the message is read
 *
 * Set the read status of @message to @read.
 */
void
valent_message_set_read (ValentMessage *message,
                         gboolean       read)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  if (message->read == read)
    return;

  message->read = read;
  g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_READ]);
}

/**
 * valent_message_get_sender:
 * @message: a #ValentMessage
 *
 * Get the sender of @message.
 *
 * Returns: (transfer none) (nullable): the message sender
 */
const char *
valent_message_get_sender (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->sender;
}

/**
 * valent_message_get_text:
 * @message: a #ValentMessage
 *
 * Get the text content of @message.
 *
 * Returns: (transfer none) (nullable): the message text
 */
const char *
valent_message_get_text (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->text;
}

/**
 * valent_message_get_thread_id:
 * @message: a #ValentMessage
 *
 * Get the thread ID @message belongs to.
 *
 * Returns: the thread ID
 */
gint64
valent_message_get_thread_id (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->thread_id;
}

/**
 * valent_message_update:
 * @message: a #ValentMessage
 * @update: (transfer full): a #ValentMessage
 *
 * Update @message with data from @update. The #ValentMessage:id property
 * must match on both objects.
 *
 * This function consumes @update and all its memory, so it should not be used
 * after calling this.
 */
void
valent_message_update (ValentMessage *message,
                       ValentMessage *update)
{
  g_return_if_fail (VALENT_IS_MESSAGE (message));
  g_return_if_fail (VALENT_IS_MESSAGE (update));
  g_return_if_fail (message->id == update->id);

  g_object_freeze_notify (G_OBJECT (message));

  if (message->box != update->box)
    {
      message->box = update->box;
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_BOX]);
    }

  if (message->date != update->date)
    {
      message->date = update->date;
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_DATE]);
    }

  if (message->metadata != update->metadata)
    {
      g_clear_pointer (&message->metadata, g_variant_unref);
      message->metadata = g_steal_pointer (&update->metadata);
    }

  if (message->read != update->read)
    {
      message->read = update->read;
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_READ]);
    }

  if (valent_set_string (&message->sender, update->sender))
    g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_SENDER]);

  if (valent_set_string (&message->text, update->text))
    g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_TEXT]);

  g_object_thaw_notify (G_OBJECT (message));
  g_object_unref (update);
}

