// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-message"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-message.h"
#include "valent-message-attachment.h"


struct _ValentMessage
{
  GObject           parent_instance;

  GListModel       *attachments;
  ValentMessageBox  box;
  int64_t           date;
  int64_t           id;
  unsigned int      read : 1;
  GStrv             recipients;
  char             *sender;
  int64_t           subscription_id;
  char             *text;
  int64_t           thread_id;
  char             *iri;
};

G_DEFINE_FINAL_TYPE (ValentMessage, valent_message, G_TYPE_OBJECT)

typedef enum {
  PROP_ATTACHMENTS = 1,
  PROP_BOX,
  PROP_DATE,
  PROP_ID,
  PROP_IRI,
  PROP_READ,
  PROP_RECIPIENTS,
  PROP_SENDER,
  PROP_SUBSCRIPTION_ID,
  PROP_TEXT,
  PROP_THREAD_ID,
} ValentMessageProperty;

static GParamSpec *properties[PROP_THREAD_ID  + 1] = { NULL, };


/*
 * GObject
 */
static void
valent_message_finalize (GObject *object)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  g_clear_object (&self->attachments);
  g_clear_pointer (&self->iri, g_free);
  g_clear_pointer (&self->sender, g_free);
  g_clear_pointer (&self->recipients, g_strfreev);
  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (valent_message_parent_class)->finalize (object);
}

static void
valent_message_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ValentMessage *self = VALENT_MESSAGE (object);

  switch ((ValentMessageProperty)prop_id)
    {
    case PROP_ATTACHMENTS:
      g_value_set_object (value, valent_message_get_attachments (self));
      break;

    case PROP_BOX:
      g_value_set_uint (value, self->box);
      break;

    case PROP_DATE:
      g_value_set_int64 (value, self->date);
      break;

    case PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case PROP_IRI:
      g_value_set_string (value, self->iri);
      break;

    case PROP_READ:
      g_value_set_boolean (value, self->read);
      break;

    case PROP_RECIPIENTS:
      g_value_set_boxed (value, self->recipients);
      break;

    case PROP_SENDER:
      g_value_set_string (value, self->sender);
      break;

    case PROP_SUBSCRIPTION_ID:
      g_value_set_int64 (value, self->subscription_id);
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

  switch ((ValentMessageProperty)prop_id)
    {
    case PROP_ATTACHMENTS:
      self->attachments = g_value_dup_object (value);
      break;

    case PROP_BOX:
      self->box = g_value_get_uint (value);
      break;

    case PROP_DATE:
      self->date = g_value_get_int64 (value);
      break;

    case PROP_ID:
      self->id = g_value_get_int64 (value);
      break;

    case PROP_IRI:
      self->iri = g_value_dup_string (value);
      break;

    case PROP_READ:
      self->read = g_value_get_boolean (value);
      break;

    case PROP_RECIPIENTS:
      self->recipients = g_value_dup_boxed (value);
      break;

    case PROP_SENDER:
      self->sender = g_value_dup_string (value);
      break;

    case PROP_SUBSCRIPTION_ID:
      self->subscription_id = g_value_get_int64 (value);
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
   * ValentMessage:attachments: (getter get_attachments)
   *
   * The list of attachments.
   *
   * Since: 1.0
   */
  properties [PROP_ATTACHMENTS] =
    g_param_spec_object ("attachments", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:box: (getter get_box)
   *
   * The `ValentMessageBox` of the message.
   *
   * Since: 1.0
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
   * ValentMessage:date: (getter get_date)
   *
   * A UNIX epoch timestamp for the message.
   *
   * Since: 1.0
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
   * ValentMessage:id: (getter get_id)
   *
   * The unique ID for this message.
   *
   * Since: 1.0
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
   * ValentMessage:iri: (getter get_iri)
   *
   * The iri of the message.
   *
   * Since: 1.0
   */
  properties [PROP_IRI] =
    g_param_spec_string ("iri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:read: (getter get_read)
   *
   * Whether the message has been read.
   *
   * Since: 1.0
   */
  properties [PROP_READ] =
    g_param_spec_boolean ("read", NULL, NULL,
                          FALSE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT_ONLY |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:recipients: (getter get_recipients)
   *
   * The recipients of the message.
   *
   * This will usually be a list of phone numbers, email addresses or some
   * other electronic medium.
   *
   * Since: 1.0
   */
  properties [PROP_RECIPIENTS] =
    g_param_spec_boxed ("recipients", NULL, NULL,
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:sender: (getter get_sender)
   *
   * The sender of the message. This will usually be a phone number, email
   * address or some other electronic medium.
   *
   * Since: 1.0
   */
  properties [PROP_SENDER] =
    g_param_spec_string ("sender", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:subscription-id: (getter get_subscription_id)
   *
   * The subscription ID for this message.
   *
   * Since: 1.0
   */
  properties [PROP_SUBSCRIPTION_ID] =
    g_param_spec_int64 ("subscription-id", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        -1,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:text: (getter get_text)
   *
   * The text content of the message.
   *
   * Since: 1.0
   */
  properties [PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessage:thread-id: (getter get_thread_id)
   *
   * The thread this message belongs to.
   *
   * Since: 1.0
   */
  properties [PROP_THREAD_ID] =
    g_param_spec_int64 ("thread-id", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_message_init (ValentMessage *message)
{
}

/**
 * valent_message_get_attachments: (get-property attachments)
 * @message: a `ValentMessage`
 *
 * Get the list of attachments.
 *
 * Returns: (transfer none) (not nullable): a `GListModel`
 *
 * Since: 1.0
 */
GListModel *
valent_message_get_attachments (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  if (message->attachments == NULL)
    {
      message->attachments =
        G_LIST_MODEL (g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT));
    }

  return message->attachments;
}

/**
 * valent_message_get_box: (get-property box)
 * @message: a `ValentMessage`
 *
 * Get the `ValentMessageBox` of @message.
 *
 * Returns: a `ValentMessageBox`
 *
 * Since: 1.0
 */
ValentMessageBox
valent_message_get_box (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), VALENT_MESSAGE_BOX_ALL);

  return message->box;
}

/**
 * valent_message_get_date: (get-property date)
 * @message: a `ValentMessage`
 *
 * Get the timestamp for @message.
 *
 * Returns: the message timestamp
 *
 * Since: 1.0
 */
int64_t
valent_message_get_date (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->date;
}

/**
 * valent_message_get_id: (get-property id)
 * @message: a `ValentMessage`
 *
 * Get the unique ID for @message.
 *
 * Returns: the message ID
 *
 * Since: 1.0
 */
int64_t
valent_message_get_id (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->id;
}

/**
 * valent_message_get_iri: (get-property iri)
 * @message: a `ValentMessage`
 *
 * Get the IRI for @message.
 *
 * Returns: (transfer none) (nullable): the message IRI
 *
 * Since: 1.0
 */
const char *
valent_message_get_iri (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->iri;
}

/**
 * valent_message_get_read: (get-property read)
 * @message: a `ValentMessage`
 *
 * Get the read status of @message.
 *
 * Returns: %TRUE if the message has been read
 *
 * Since: 1.0
 */
gboolean
valent_message_get_read (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), FALSE);

  return message->read;
}

/**
 * valent_message_get_recipients: (get-property recipients)
 * @message: a `ValentMessage`
 *
 * Get the recipients of @message.
 *
 * Returns: (transfer none) (nullable): the message recipients
 *
 * Since: 1.0
 */
const char * const *
valent_message_get_recipients (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return (const char * const *)message->recipients;
}

/**
 * valent_message_get_sender: (get-property sender)
 * @message: a `ValentMessage`
 *
 * Get the sender of @message.
 *
 * Returns: (transfer none) (nullable): the message sender
 *
 * Since: 1.0
 */
const char *
valent_message_get_sender (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->sender;
}

/**
 * valent_message_get_subscription_id: (get-property subscription-id)
 * @message: a `ValentMessage`
 *
 * Get the subscription ID for @message.
 *
 * Returns: the subscription ID
 *
 * Since: 1.0
 */
int64_t
valent_message_get_subscription_id (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->subscription_id;
}

/**
 * valent_message_get_text: (get-property text)
 * @message: a `ValentMessage`
 *
 * Get the text content of @message.
 *
 * Returns: (transfer none) (nullable): the message text
 *
 * Since: 1.0
 */
const char *
valent_message_get_text (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), NULL);

  return message->text;
}

/**
 * valent_message_get_thread_id: (get-property thread-id)
 * @message: a `ValentMessage`
 *
 * Get the thread ID @message belongs to.
 *
 * Returns: the thread ID
 *
 * Since: 1.0
 */
int64_t
valent_message_get_thread_id (ValentMessage *message)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE (message), 0);

  return message->thread_id;
}

/**
 * valent_message_update:
 * @message: a `ValentMessage`
 * @update: (transfer full): a `ValentMessage`
 *
 * Update @message with data from @update. The `ValentMessage`:id property
 * must match on both objects.
 *
 * This function consumes @update and all its memory, so it should not be used
 * after calling this.
 *
 * Since: 1.0
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

  if (message->read != update->read)
    {
      message->read = update->read;
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_READ]);
    }

  if (message->recipients != update->recipients)
    {
      g_clear_pointer (&message->recipients, g_strfreev);
      message->recipients = g_steal_pointer (&update->recipients);
      g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_RECIPIENTS]);
    }

  if (g_set_str (&message->sender, update->sender))
    g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_SENDER]);

  if (g_set_str (&message->text, update->text))
    g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_TEXT]);

  if (g_set_object (&message->attachments, update->attachments))
    g_object_notify_by_pspec (G_OBJECT (message), properties [PROP_ATTACHMENTS]);

  g_object_thaw_notify (G_OBJECT (message));
  g_object_unref (update);
}

