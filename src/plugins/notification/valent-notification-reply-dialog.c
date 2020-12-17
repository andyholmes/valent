// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-reply-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

#include "valent-notification-reply-dialog.h"


struct _ValentNotificationReplyDialog
{
  GtkDialog           parent_instance;

  ValentNotification *notification;
  char               *uuid;

  /* Template widgets */
  GtkInfoBar         *infobar;
  GtkButton          *cancel_button;
  GtkButton          *send_button;
  GtkLabel           *title_label;
  GtkLabel           *body_label;
  GtkTextView        *reply_entry;
};

G_DEFINE_TYPE (ValentNotificationReplyDialog, valent_notification_reply_dialog, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_NOTIFICATION,
  PROP_UUID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_notification_reply_dialog_constructed (GObject *object)
{
  ValentNotificationReplyDialog *self = VALENT_NOTIFICATION_REPLY_DIALOG (object);

  if (self->notification != NULL)
    {
      const char *title;
      const char *body;

      title = valent_notification_get_title (self->notification);

      if (title != NULL)
        gtk_label_set_label (self->title_label, title);

      body = valent_notification_get_body (self->notification);

      if (body != NULL)
        gtk_label_set_label (self->body_label, body);
    }

  G_OBJECT_CLASS (valent_notification_reply_dialog_parent_class)->constructed (object);
}

static void
valent_notification_reply_dialog_finalize (GObject *object)
{
  ValentNotificationReplyDialog *self = VALENT_NOTIFICATION_REPLY_DIALOG (object);

  g_clear_object (&self->notification);
  g_clear_pointer (&self->uuid, g_free);

  G_OBJECT_CLASS (valent_notification_reply_dialog_parent_class)->finalize (object);
}

static void
valent_notification_reply_dialog_get_property (GObject    *object,
                                               guint       prop_id,
                                               GValue     *value,
                                               GParamSpec *pspec)
{
  ValentNotificationReplyDialog *self = VALENT_NOTIFICATION_REPLY_DIALOG (object);

  switch (prop_id)
    {
    case PROP_NOTIFICATION:
      g_value_set_object (value, self->notification);
      break;

    case PROP_UUID:
      g_value_set_string (value, self->uuid);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_reply_dialog_set_property (GObject      *object,
                                               guint         prop_id,
                                               const GValue *value,
                                               GParamSpec   *pspec)
{
  ValentNotificationReplyDialog *self = VALENT_NOTIFICATION_REPLY_DIALOG (object);

  switch (prop_id)
    {
    case PROP_NOTIFICATION:
      self->notification = g_value_dup_object (value);
      break;

    case PROP_UUID:
      valent_notification_reply_dialog_set_uuid (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_reply_dialog_class_init (ValentNotificationReplyDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_notification_reply_dialog_constructed;
  object_class->finalize = valent_notification_reply_dialog_finalize;
  object_class->get_property = valent_notification_reply_dialog_get_property;
  object_class->set_property = valent_notification_reply_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/notification/valent-notification-reply-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationReplyDialog, infobar);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationReplyDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationReplyDialog, send_button);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationReplyDialog, title_label);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationReplyDialog, body_label);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationReplyDialog, reply_entry);

  /**
   * ValentNotificationReplyDialog:notification:
   *
   * The notification being replied to.
   */
  properties[PROP_NOTIFICATION] =
    g_param_spec_object ("notification",
                         "Notification",
                         "The notification being replied to",
                         VALENT_TYPE_NOTIFICATION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotificationReplyDialog:uuid:
   *
   * The notification reply UUID.
   */
  properties[PROP_UUID] =
    g_param_spec_string ("uuid",
                         "UUID",
                         "The notification reply UUID",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));
}

static void
valent_notification_reply_dialog_init (ValentNotificationReplyDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * valent_notification_reply_dialog_new:
 *
 * Create a new #ValentNotificationReplyDialog.
 *
 * Returns: (transfer full): a #GtkDialog
 */
GtkDialog *
valent_notification_reply_dialog_new (ValentNotification *notification)
{
  return g_object_new (VALENT_TYPE_NOTIFICATION_REPLY_DIALOG,
                       "notification",   notification,
                       "use-header-bar", TRUE,
                       NULL);
}

/**
 * valent_notification_reply_dialog_get_text:
 * @dialog: a #ValentNotificationReplyDialog
 *
 * Get the command text entry text for @dialog
 *
 * Returns: (transfer full): the command text
 */
char *
valent_notification_reply_dialog_get_text (ValentNotificationReplyDialog *dialog)
{
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  g_return_val_if_fail (VALENT_IS_NOTIFICATION_REPLY_DIALOG (dialog), NULL);

  buffer = gtk_text_view_get_buffer (dialog->reply_entry);
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_get_end_iter (buffer, &end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

/**
 * valent_notification_reply_dialog_set_text:
 * @dialog: a #ValentNotificationReplyDialog
 * @text: a command text
 *
 * Set the command text entry text for @dialog to @command.
 */
void
valent_notification_reply_dialog_set_text (ValentNotificationReplyDialog *dialog,
                                           const char                    *text)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (VALENT_IS_NOTIFICATION_REPLY_DIALOG (dialog));

  buffer = gtk_text_view_get_buffer (dialog->reply_entry);

  gtk_text_buffer_set_text (buffer, text, -1);
}

/**
 * valent_notification_reply_dialog_get_uuid:
 * @dialog: a #ValentNotificationReplyDialog
 *
 * Get the notification reply UUID for @dialog.
 *
 * Returns: (transfer none): the reply UUID
 */
const char *
valent_notification_reply_dialog_get_uuid (ValentNotificationReplyDialog *dialog)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION_REPLY_DIALOG (dialog), NULL);

  return dialog->uuid;
}

/**
 * valent_notification_reply_dialog_set_text:
 * @dialog: a #ValentNotificationReplyDialog
 * @uuid: a UUID
 *
 * Set the notification reply UUID for @dialog to @uuid.
 */
void
valent_notification_reply_dialog_set_uuid (ValentNotificationReplyDialog *dialog,
                                           const char                    *uuid)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION_REPLY_DIALOG (dialog));

  if (g_strcmp0 (dialog->uuid, uuid) == 0)
    return;

  g_clear_pointer (&dialog->uuid, g_free);
  dialog->uuid = g_strdup (uuid);
  g_object_notify_by_pspec (G_OBJECT (dialog), properties[PROP_UUID]);
}

