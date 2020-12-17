// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-message"

#include "config.h"

#include <gio/gio.h>

#include "valent-sms-message.h"


struct _ValentSmsMessage
{
  GObject              parent_instance;

  ValentSmsMessageBox  box;
  gint64               date;
  gint64               id;
  GVariant            *metadata;
  gboolean             read;
  char                *sender;
  char                *text;
  gint64               thread_id;
};

G_DEFINE_TYPE (ValentSmsMessage, valent_sms_message, G_TYPE_OBJECT)

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


static void
valent_sms_message_finalize (GObject *object)
{
  ValentSmsMessage *self = VALENT_SMS_MESSAGE (object);

  g_clear_pointer (&self->sender, g_free);
  g_clear_pointer (&self->text, g_free);
  g_clear_pointer (&self->metadata, g_variant_unref);

  G_OBJECT_CLASS (valent_sms_message_parent_class)->finalize (object);
}

static void
valent_sms_message_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentSmsMessage *self = VALENT_SMS_MESSAGE (object);

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
      g_value_set_variant (value, self->metadata);
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
      g_value_set_int64 (value, self->date);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_message_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentSmsMessage *self = VALENT_SMS_MESSAGE (object);

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
      self->read = g_value_get_boolean (value);
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
valent_sms_message_init (ValentSmsMessage *message)
{
}

static void
valent_sms_message_class_init (ValentSmsMessageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_sms_message_finalize;
  object_class->get_property = valent_sms_message_get_property;
  object_class->set_property = valent_sms_message_set_property;

  /**
   * ValentSmsMessage:box:
   *
   * The #ValentSmsMessageBox of the message.
   */
  properties [PROP_BOX] =
    g_param_spec_uint ("box",
                       "Category",
                        "The ValentSmsMessageBox of the message",
                        VALENT_SMS_MESSAGE_BOX_ALL, VALENT_SMS_MESSAGE_BOX_FAILED,
                        VALENT_SMS_MESSAGE_BOX_ALL,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsMessage:date:
   *
   * A UNIX epoch timestamp for the message.
   */
  properties [PROP_DATE] =
    g_param_spec_int64 ("date",
                        "Date",
                        "Integer indicating the date",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsMessage:id:
   *
   * The unique ID for this message.
   */
  properties [PROP_ID] =
    g_param_spec_int64 ("id",
                        "ID",
                        "Unique ID for this message",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsMessage:metadata:
   *
   * Ancillary data for the message, such as media.
   */
  properties [PROP_METADATA] =
    g_param_spec_variant ("metadata",
                          "Metadata",
                          "Ancillary data for the message",
                          G_VARIANT_TYPE_DICTIONARY,
                          NULL,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsMessage:read:
   *
   * Whether the message has been read.
   */
  properties [PROP_READ] =
    g_param_spec_boolean ("read",
                          "Read",
                          "Whether the message has been read",
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsMessage:sender:
   *
   * The sender of the message. This will usually be a phone number or other
   * address form.
   */
  properties [PROP_SENDER] =
    g_param_spec_string ("sender",
                         "Sender",
                         "The sender of the message",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsMessage:text:
   *
   * The text content of the message.
   */
  properties [PROP_TEXT] =
    g_param_spec_string ("text",
                         "Text",
                         "The text content of the message",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsMessage:thread-id:
   *
   * The thread this message belongs to.
   */
  properties [PROP_THREAD_ID] =
    g_param_spec_int64 ("thread-id",
                        "Thread ID",
                        "The thread this message belongs to",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * valent_sms_message_get_box:
 * @message: a #ValentSmsMessage
 *
 * Get the #ValentSmsMessageBox of @message.
 *
 * Returns: a #ValentSmsMessageBox
 */
ValentSmsMessageBox
valent_sms_message_get_box (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), VALENT_SMS_MESSAGE_BOX_ALL);

  return message->box;
}

/**
 * valent_sms_message_get_date:
 * @message: a #ValentSmsMessage
 *
 * Get the timestamp for @message.
 *
 * Returns: the message timestamp
 */
gint64
valent_sms_message_get_date (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), 0);

  return message->date;
}

/**
 * valent_sms_message_get_id:
 * @message: a #ValentSmsMessage
 *
 * Get the unique ID for @message.
 *
 * Returns: the message ID
 */
gint64
valent_sms_message_get_id (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), 0);

  return message->id;
}

/**
 * valent_sms_message_get_metadata:
 * @message: a #ValentSmsMessage
 *
 * Get the #GVariant dictionary of metadata.
 *
 * Returns: (transfer none) (nullable): the metadata
 */
GVariant *
valent_sms_message_get_metadata (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), NULL);

  return message->metadata;
}

/**
 * valent_sms_message_get_read:
 * @message: a #ValentSmsMessage
 *
 * Get the read status of @message.
 *
 * Returns: %TRUE if the message has been read
 */
gboolean
valent_sms_message_get_read (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), FALSE);

  return message->read;
}

/**
 * valent_sms_message_get_sender:
 * @message: a #ValentSmsMessage
 *
 * Get the sender of @message.
 *
 * Returns: (transfer none) (nullable): the message sender
 */
const char *
valent_sms_message_get_sender (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), NULL);

  return message->sender;
}

/**
 * valent_sms_message_set_sender:
 * @message: a #ValentSmsMessage
 * @sender: a phone number or other address
 *
 * Set the sender of @message.
 */
void
valent_sms_message_set_sender (ValentSmsMessage *message,
                               const char       *sender)
{
  g_return_if_fail (VALENT_IS_SMS_MESSAGE (message));

  if (message->sender != NULL)
    g_clear_pointer (&message->sender, g_free);

  message->sender = g_strdup (sender);
}

/**
 * valent_sms_message_get_text:
 * @message: a #ValentSmsMessage
 *
 * Get the text content of @message.
 *
 * Returns: (transfer none) (nullable): the message text
 */
const char *
valent_sms_message_get_text (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), NULL);

  return message->text;
}

/**
 * valent_sms_message_get_thread_id:
 * @message: a #ValentSmsMessage
 *
 * Get the thread ID @message belongs to.
 *
 * Returns: the thread ID
 */
gint64
valent_sms_message_get_thread_id (ValentSmsMessage *message)
{
  g_return_val_if_fail (VALENT_IS_SMS_MESSAGE (message), 0);

  return message->thread_id;
}

