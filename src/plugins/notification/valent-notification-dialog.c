// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>
#include <libvalent-notifications.h>

#include "valent-notification-dialog.h"


struct _ValentNotificationDialog
{
  GtkDialog           parent_instance;

  ValentNotification *notification;
  char               *reply_id;
  gboolean            state;

  /* Template widgets */
  GtkButton          *cancel_button;
  GtkButton          *send_button;
  GtkImage           *icon_image;
  GtkLabel           *title_label;
  GtkLabel           *body_label;
  GtkWidget          *reply_frame;
  GtkTextView        *reply_entry;
};

G_DEFINE_TYPE (ValentNotificationDialog, valent_notification_dialog, GTK_TYPE_DIALOG)

enum {
  PROP_0,
  PROP_NOTIFICATION,
  PROP_REPLY_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
valent_notification_dialog_check (ValentNotificationDialog *self)
{
  gboolean state = FALSE;

  if (self->state)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (self->reply_entry);
      state = gtk_text_buffer_get_char_count (buffer) > 0;
    }

  gtk_dialog_set_response_sensitive (GTK_DIALOG (self), GTK_RESPONSE_OK, state);
}
static void
valent_notification_dialog_set_notification (ValentNotificationDialog *self,
                                             ValentNotification       *notification)
{
  g_assert (VALENT_IS_NOTIFICATION_DIALOG (self));
  g_assert (notification == NULL || VALENT_IS_NOTIFICATION (notification));

  if (g_set_object (&self->notification, notification))
    {
      GIcon *icon;
      const char *title;
      const char *body;

      icon = valent_notification_get_icon (self->notification);

      if (icon != NULL)
        gtk_image_set_from_gicon (self->icon_image, icon);

      title = valent_notification_get_title (self->notification);

      if (title != NULL)
        gtk_label_set_label (self->title_label, title);

      body = valent_notification_get_body (self->notification);

      if (body != NULL)
        gtk_label_set_label (self->body_label, body);
    }

  valent_notification_dialog_check (self);
}

/*
 * GObject
 */
static void
valent_notification_dialog_finalize (GObject *object)
{
  ValentNotificationDialog *self = VALENT_NOTIFICATION_DIALOG (object);

  g_clear_object (&self->notification);
  g_clear_pointer (&self->reply_id, g_free);

  G_OBJECT_CLASS (valent_notification_dialog_parent_class)->finalize (object);
}

static void
valent_notification_dialog_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  ValentNotificationDialog *self = VALENT_NOTIFICATION_DIALOG (object);

  switch (prop_id)
    {
    case PROP_NOTIFICATION:
      g_value_set_object (value, self->notification);
      break;

    case PROP_REPLY_ID:
      g_value_set_string (value, self->reply_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_dialog_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  ValentNotificationDialog *self = VALENT_NOTIFICATION_DIALOG (object);

  switch (prop_id)
    {
    case PROP_NOTIFICATION:
      valent_notification_dialog_set_notification (self, g_value_get_object (value));
      break;

    case PROP_REPLY_ID:
      valent_notification_dialog_set_reply_id (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_notification_dialog_class_init (ValentNotificationDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_notification_dialog_finalize;
  object_class->get_property = valent_notification_dialog_get_property;
  object_class->set_property = valent_notification_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/notification/valent-notification-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, send_button);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, icon_image);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, title_label);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, body_label);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, reply_frame);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, reply_entry);

  /**
   * ValentNotificationDialog:notification:
   *
   * The notification the dialog represents.
   */
  properties[PROP_NOTIFICATION] =
    g_param_spec_object ("notification",
                         "Notification",
                         "The notification the dialog represents",
                         VALENT_TYPE_NOTIFICATION,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotificationDialog:reply-id:
   *
   * The notification reply ID.
   */
  properties[PROP_REPLY_ID] =
    g_param_spec_string ("reply-id",
                         "Reply ID",
                         "The notification reply ID",
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_notification_dialog_init (ValentNotificationDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_swapped (gtk_text_view_get_buffer (self->reply_entry),
                            "notify::text",
                            G_CALLBACK (valent_notification_dialog_check),
                            self);
}

/**
 * valent_notification_dialog_new:
 * @notification: a #ValentNotification
 *
 * Create a new #ValentNotificationDialog.
 *
 * Returns: (transfer full) (nullable): a #GtkDialog
 */
GtkDialog *
valent_notification_dialog_new (ValentNotification *notification)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION (notification), NULL);

  return g_object_new (VALENT_TYPE_NOTIFICATION_DIALOG,
                       "notification",   notification,
                       "use-header-bar", TRUE,
                       NULL);
}

/**
 * valent_notification_dialog_get_notification:
 * @dialog: a #ValentNotificationDialog
 *
 * Get the #ValentNotification @dialog represents.
 *
 * Returns: (transfer none): a #ValentNotification
 */
ValentNotification *
valent_notification_dialog_get_notification (ValentNotificationDialog *dialog)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION_DIALOG (dialog), NULL);

  return dialog->notification;
}

/**
 * valent_notification_dialog_get_reply_id:
 * @dialog: a #ValentNotificationDialog
 *
 * Get the notification reply ID for @dialog.
 *
 * Returns: (transfer none): the reply ID
 */
const char *
valent_notification_dialog_get_reply_id (ValentNotificationDialog *dialog)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION_DIALOG (dialog), NULL);

  return dialog->reply_id;
}

/**
 * valent_notification_dialog_set_reply_id:
 * @dialog: a #ValentNotificationDialog
 * @reply_id: (nullable): a notification reply ID
 *
 * Set the notification reply ID for @dialog.
 *
 * If @reply_id is %NULL or empty, the notification can not be replied to.
 */
void
valent_notification_dialog_set_reply_id (ValentNotificationDialog *dialog,
                                         const char               *reply_id)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION_DIALOG (dialog));

  if (g_strcmp0 (dialog->reply_id, reply_id) == 0)
    return;

  g_clear_pointer (&dialog->reply_id, g_free);
  dialog->reply_id = g_strdup (reply_id);
  g_object_notify_by_pspec (G_OBJECT (dialog), properties [PROP_REPLY_ID]);

  if (reply_id == NULL || *reply_id == '\0')
    {
      gtk_widget_set_visible (GTK_WIDGET (dialog->reply_frame), FALSE);
      gtk_widget_set_visible (GTK_WIDGET (dialog->send_button), FALSE);
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (dialog->reply_frame), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (dialog->send_button), TRUE);
    }
}

/**
 * valent_notification_dialog_get_reply:
 * @dialog: a #ValentNotificationDialog
 *
 * Get the message @dialog
 *
 * Returns: (transfer full): the command text
 */
char *
valent_notification_dialog_get_reply (ValentNotificationDialog *dialog)
{
  GtkTextBuffer *buffer;
  GtkTextIter start, end;

  g_return_val_if_fail (VALENT_IS_NOTIFICATION_DIALOG (dialog), NULL);

  buffer = gtk_text_view_get_buffer (dialog->reply_entry);
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

/**
 * valent_notification_dialog_update_state:
 * @dialog: a #ValentNotificationDialog
 * @state: the #ValentDevice state
 *
 * Sets whether the associated #ValentDevice is available (ie. connected and
 * paired).
 */
void
valent_notification_dialog_update_state (ValentNotificationDialog *dialog,
                                         gboolean                  state)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION_DIALOG (dialog));

  dialog->state = state;
  valent_notification_dialog_check (dialog);
}

