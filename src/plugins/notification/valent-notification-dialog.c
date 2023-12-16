// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-notification-dialog"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-notification-dialog.h"


struct _ValentNotificationDialog
{
  GtkWindow           parent_instance;

  ValentDevice       *device;
  ValentNotification *notification;
  char               *reply_id;

  /* template */
  GtkButton          *cancel_button;
  GtkButton          *reply_button;
  GtkImage           *icon_image;
  GtkLabel           *title_label;
  GtkLabel           *body_label;
  GtkLabel           *time_label;
  GtkWidget          *reply_frame;
  GtkTextView        *reply_entry;
};

G_DEFINE_FINAL_TYPE (ValentNotificationDialog, valent_notification_dialog, GTK_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_NOTIFICATION,
  PROP_REPLY_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
valent_notification_dialog_sync (ValentNotificationDialog *self)
{
  gboolean repliable = FALSE;
  gboolean enabled = FALSE;

  g_assert (VALENT_IS_NOTIFICATION_DIALOG (self));

  /* Check if the notification is repliable */
  repliable = (self->reply_id != NULL && *self->reply_id != '\0');
  gtk_widget_set_visible (GTK_WIDGET (self->reply_button), repliable);
  gtk_widget_set_visible (GTK_WIDGET (self->reply_frame), repliable);

  /* If it's repliable, check if the device action is enabled */
  if (repliable && self->device != NULL)
    enabled = g_action_group_get_action_enabled (G_ACTION_GROUP (self->device),
                                                 "notification.reply");

  gtk_widget_set_sensitive (GTK_WIDGET (self->reply_entry), enabled);

  /* If it's enabled, check if a reply is ready to be sent */
  if (enabled)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (self->reply_entry);
      enabled = gtk_text_buffer_get_char_count (buffer) > 0;
    }

  gtk_widget_action_set_enabled (GTK_WIDGET (self),
                                 "notification.reply",
                                 enabled);
}

static void
valent_notification_dialog_set_notification (ValentNotificationDialog *self,
                                             ValentNotification       *notification)
{
  GIcon *icon;
  const char *title;
  const char *body;
  gint64 timestamp;

  g_assert (VALENT_IS_NOTIFICATION_DIALOG (self));
  g_assert (notification == NULL || VALENT_IS_NOTIFICATION (notification));

  if (!g_set_object (&self->notification, notification))
    return;

  if ((icon = valent_notification_get_icon (self->notification)) != NULL)
    gtk_image_set_from_gicon (self->icon_image, icon);

  if ((title = valent_notification_get_title (self->notification)) != NULL)
    gtk_label_set_label (self->title_label, title);

  if ((body = valent_notification_get_body (self->notification)) != NULL)
    {
      g_autofree char *label = NULL;

      label = valent_string_to_markup (body);
      gtk_label_set_label (self->body_label, label);
    }

      // FIXME: better time string
  if ((timestamp = valent_notification_get_time (self->notification)) != 0)
    {
      g_autoptr (GDateTime) date = NULL;
      g_autofree char *label = NULL;

      date = g_date_time_new_from_unix_local (timestamp / 1000);
      label = g_date_time_format (date, "%c");
      gtk_label_set_label (self->time_label, label);
    }

  valent_notification_dialog_sync (self);
}

/*
 * GAction
 */
static void
dialog_cancel_action (GtkWidget  *widget,
                      const char *action_name,
                      GVariant   *parameter)
{
  ValentNotificationDialog *self = VALENT_NOTIFICATION_DIALOG (widget);

  g_assert (VALENT_IS_NOTIFICATION_DIALOG (self));

  gtk_window_destroy (GTK_WINDOW (self));
}

static void
notification_close_action (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *parameter)
{
  ValentNotificationDialog *self = VALENT_NOTIFICATION_DIALOG (widget);
  const char *id = NULL;

  g_assert (VALENT_IS_NOTIFICATION_DIALOG (self));

  id = valent_notification_get_id (self->notification);
  g_action_group_activate_action (G_ACTION_GROUP (self->device),
                                  "notification.close",
                                  g_variant_new_string (id));
}

static void
notification_reply_action (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *parameter)
{
  ValentNotificationDialog *self = VALENT_NOTIFICATION_DIALOG (widget);
  g_autofree char *message = NULL;
  GVariant *reply = NULL;
  GtkTextBuffer *buffer;

  if (!g_action_group_get_action_enabled (G_ACTION_GROUP (self->device),
                                          "notification.reply"))
    return;

  buffer = gtk_text_view_get_buffer (self->reply_entry);
  g_object_get (buffer, "text", &message, NULL);
  g_return_if_fail (message != NULL && *message != '\0');

  reply = g_variant_new ("(ssv)", self->reply_id, message,
                         valent_notification_serialize (self->notification));
  g_action_group_activate_action (G_ACTION_GROUP (self->device),
                                  "notification.reply",
                                  reply);
  gtk_window_destroy (GTK_WINDOW (self));
}

/*
 * GObject
 */
static void
valent_notification_dialog_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_NOTIFICATION_DIALOG);

  G_OBJECT_CLASS (valent_notification_dialog_parent_class)->dispose (object);
}

static void
valent_notification_dialog_constructed (GObject *object)
{
  ValentNotificationDialog *self = VALENT_NOTIFICATION_DIALOG (object);

  if (self->device != NULL)
    {
      gtk_widget_insert_action_group (GTK_WIDGET (self),
                                      "device",
                                      G_ACTION_GROUP (self->device));
      g_signal_connect_object (self->device,
                               "action-added::notification.reply",
                               G_CALLBACK (valent_notification_dialog_sync),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->device,
                               "action-removed::notification.reply",
                               G_CALLBACK (valent_notification_dialog_sync),
                               self,
                               G_CONNECT_SWAPPED);
      g_signal_connect_object (self->device,
                               "action-enabled-changed::notification.reply",
                               G_CALLBACK (valent_notification_dialog_sync),
                               self,
                               G_CONNECT_SWAPPED);
    }

  valent_notification_dialog_sync (self);

  G_OBJECT_CLASS (valent_notification_dialog_parent_class)->constructed (object);
}

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
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

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
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

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

  object_class->dispose = valent_notification_dialog_dispose;
  object_class->constructed = valent_notification_dialog_constructed;
  object_class->finalize = valent_notification_dialog_finalize;
  object_class->get_property = valent_notification_dialog_get_property;
  object_class->set_property = valent_notification_dialog_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/notification/valent-notification-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, reply_button);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, icon_image);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, title_label);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, body_label);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, time_label);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, reply_frame);
  gtk_widget_class_bind_template_child (widget_class, ValentNotificationDialog, reply_entry);

  gtk_widget_class_install_action (widget_class, "dialog.cancel", NULL, dialog_cancel_action);

  gtk_widget_class_install_action (widget_class, "notification.close", NULL, notification_close_action);
  gtk_widget_class_install_action (widget_class, "notification.reply", NULL, notification_reply_action);

  /**
   * ValentNotificationDialog:device:
   *
   * The device that owns the notification.
   */
  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentNotificationDialog:notification:
   *
   * The notification the dialog represents.
   */
  properties [PROP_NOTIFICATION] =
    g_param_spec_object ("notification", NULL, NULL,
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
  properties [PROP_REPLY_ID] =
    g_param_spec_string ("reply-id", NULL, NULL,
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

  g_signal_connect_object (gtk_text_view_get_buffer (self->reply_entry),
                           "notify::text",
                           G_CALLBACK (valent_notification_dialog_sync),
                           self,
                           G_CONNECT_SWAPPED);
}

/**
 * valent_notification_dialog_get_notification:
 * @dialog: a `ValentNotificationDialog`
 *
 * Get the notification.
 *
 * Returns: (transfer none): a `ValentNotification`
 */
ValentNotification *
valent_notification_dialog_get_notification (ValentNotificationDialog *dialog)
{
  g_return_val_if_fail (VALENT_IS_NOTIFICATION_DIALOG (dialog), NULL);

  return dialog->notification;
}

/**
 * valent_notification_dialog_get_reply_id: (get-property reply-id)
 * @dialog: a `ValentNotificationDialog`
 *
 * Get the notification reply ID.
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
 * valent_notification_dialog_set_reply_id: (set-property reply-id)
 * @dialog: a `ValentNotificationDialog`
 * @reply_id: (nullable): a notification reply ID
 *
 * Set the notification reply ID.
 *
 * If @reply_id is %NULL or empty, the notification can not be replied to.
 */
void
valent_notification_dialog_set_reply_id (ValentNotificationDialog *dialog,
                                         const char               *reply_id)
{
  g_return_if_fail (VALENT_IS_NOTIFICATION_DIALOG (dialog));

  if (!g_set_str (&dialog->reply_id, reply_id))
    return;

  valent_notification_dialog_sync (dialog);
  g_object_notify_by_pspec (G_OBJECT (dialog), properties [PROP_REPLY_ID]);
}

