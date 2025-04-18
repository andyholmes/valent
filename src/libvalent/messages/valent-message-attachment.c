// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-message-attachment"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-message-attachment.h"

/**
 * ValentMessageAttachment:
 *
 * A class for representing a message attachment.
 *
 * `ValentMessageAttachment is a class for representing a message attachment,
 * typically a small file or image with preview.
 *
 * Since: 1.0
 */
struct _ValentMessageAttachment
{
  ValentResource  parent_instance;

  GFile          *file;
  GIcon          *preview;
};

G_DEFINE_FINAL_TYPE (ValentMessageAttachment, valent_message_attachment, VALENT_TYPE_RESOURCE)

typedef enum {
  PROP_FILE = 1,
  PROP_PREVIEW,
} ValentMessageAttachmentProperty;

static GParamSpec *properties[PROP_PREVIEW + 1] = { NULL, };


/*
 * GObject
 */
static void
valent_message_attachment_finalize (GObject *object)
{
  ValentMessageAttachment *self = VALENT_MESSAGE_ATTACHMENT (object);

  g_clear_object (&self->file);
  g_clear_object (&self->preview);

  G_OBJECT_CLASS (valent_message_attachment_parent_class)->finalize (object);
}

static void
valent_message_attachment_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  ValentMessageAttachment *self = VALENT_MESSAGE_ATTACHMENT (object);

  switch ((ValentMessageAttachmentProperty)prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_PREVIEW:
      g_value_set_object (value, self->preview);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_attachment_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  ValentMessageAttachment *self = VALENT_MESSAGE_ATTACHMENT (object);

  switch ((ValentMessageAttachmentProperty)prop_id)
    {
    case PROP_FILE:
      valent_message_attachment_set_file (self, g_value_get_object (value));
      break;

    case PROP_PREVIEW:
      valent_message_attachment_set_preview (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_attachment_class_init (ValentMessageAttachmentClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_message_attachment_finalize;
  object_class->get_property = valent_message_attachment_get_property;
  object_class->set_property = valent_message_attachment_set_property;

  /**
   * ValentMessageAttachment:file: (getter get_file) (setter set_file)
   *
   * A file for the attachment.
   *
   * Since: 1.0
   */
  properties [PROP_FILE] =
    g_param_spec_object ("file", NULL, NULL,
                         G_TYPE_FILE,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessageAttachment:preview: (getter get_preview) (setter set_preview)
   *
   * A thumbnail preview of the attachment.
   *
   * Since: 1.0
   */
  properties [PROP_PREVIEW] =
    g_param_spec_object ("preview", NULL, NULL,
                         G_TYPE_ICON,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_message_attachment_init (ValentMessageAttachment *self)
{
}

/**
 * valent_message_attachment_get_file: (get-property file)
 * @attachment: a `ValentMessageAttachment`
 *
 * Get the file for @attachment.
 *
 * Returns: (transfer none) (nullable): the attachment file
 *
 * Since: 1.0
 */
GFile *
valent_message_attachment_get_file (ValentMessageAttachment *attachment)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_ATTACHMENT (attachment), NULL);

  return attachment->file;
}

/**
 * valent_message_attachment_set_file: (set-property file)
 * @attachment: a `ValentMessageAttachment`
 * @file: (nullable): the attachment file
 *
 * Set the file for @attachment to @file.
 *
 * Since: 1.0
 */
void
valent_message_attachment_set_file (ValentMessageAttachment *attachment,
                                    GFile                   *file)
{
  g_return_if_fail (VALENT_IS_MESSAGE_ATTACHMENT (attachment));
  g_return_if_fail (file == NULL || G_IS_FILE (file));

  if (g_set_object (&attachment->file, file))
    g_object_notify_by_pspec (G_OBJECT (attachment), properties[PROP_FILE]);
}

/**
 * valent_message_attachment_get_preview: (get-property preview)
 * @attachment: a `ValentMessageAttachment`
 *
 * Get the thumbnail preview of @attachment.
 *
 * Returns: (transfer none) (nullable): a thumbnail preview
 *
 * Since: 1.0
 */
GIcon *
valent_message_attachment_get_preview (ValentMessageAttachment *attachment)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_ATTACHMENT (attachment), NULL);

  return attachment->preview;
}

/**
 * valent_message_attachment_set_preview: (set-property preview)
 * @attachment: a `ValentMessageAttachment`
 * @preview: (nullable): the attachment preview
 *
 * Set the preview for @attachment to @preview.
 *
 * Since: 1.0
 */
void
valent_message_attachment_set_preview (ValentMessageAttachment *attachment,
                                       GIcon                   *preview)
{
  g_return_if_fail (VALENT_IS_MESSAGE_ATTACHMENT (attachment));

  if (g_set_object (&attachment->preview, preview))
    g_object_notify_by_pspec (G_OBJECT (attachment), properties[PROP_PREVIEW]);
}

