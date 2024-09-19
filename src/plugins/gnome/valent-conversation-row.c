// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-conversation-row"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "valent-date-label.h"
#include "valent-ui-utils-private.h"

#include "valent-conversation-row.h"


struct _ValentConversationRow
{
  GtkListBoxRow  parent_instance;

  ValentMessage *message;
  EContact      *contact;
  unsigned int   incoming : 1;

  /* template */
  GtkWidget     *layout;
  GtkWidget     *avatar;
  GtkWidget     *sender_label;
  GtkWidget     *attachment_list;
  GtkWidget     *message_bubble;
  GtkWidget     *summary_label;
  GtkWidget     *body_label;
  GtkWidget     *date_label;
  GtkWidget     *context_menu;
};

G_DEFINE_FINAL_TYPE (ValentConversationRow, valent_conversation_row, GTK_TYPE_LIST_BOX_ROW)


typedef enum {
  PROP_CONTACT = 1,
  PROP_DATE,
  PROP_MESSAGE,
} ValentConversationRowProperty;

static GParamSpec *properties[PROP_MESSAGE + 1] = { NULL, };


/* LCOV_EXCL_START */
static gboolean
on_activate_link (GtkLabel   *label,
                  const char *uri,
                  gpointer    user_data)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkWindow *toplevel = GTK_WINDOW (gtk_widget_get_root (widget));
  g_autoptr (GtkUriLauncher) launcher = NULL;
  g_autofree char *url = NULL;

  /* Only handle links that need to be amended with a scheme */
  if (g_uri_peek_scheme (uri) != NULL)
    return FALSE;

  if (!GTK_IS_WINDOW (toplevel))
    return FALSE;

  url = g_strdup_printf ("https://%s", uri);
  launcher = gtk_uri_launcher_new (url);
  gtk_uri_launcher_launch (launcher, toplevel, NULL, NULL, NULL);

  return TRUE;
}

static void
on_menu_popup (GtkGestureClick       *gesture,
               unsigned int           n_press,
               double                 x,
               double                 y,
               ValentConversationRow *self)
{
  gtk_popover_set_pointing_to (GTK_POPOVER (self->context_menu),
                               &(GdkRectangle){ x, y });
  gtk_popover_popup (GTK_POPOVER (self->context_menu));
}
/* LCOV_EXCL_STOP */

static void
clipboard_copy_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *parameter)
{
  ValentConversationRow *self = VALENT_CONVERSATION_ROW (widget);
  const char *text;

  g_assert (VALENT_IS_CONVERSATION_ROW (self));

  text = gtk_label_get_text (GTK_LABEL (self->body_label));
  if (text != NULL && *text != '\0')
    gdk_clipboard_set_text (gtk_widget_get_clipboard (widget), text);
}

static void
menu_popup_action (GtkWidget  *widget,
                   const char *action_name,
                   GVariant   *parameter)
{
  ValentConversationRow *self = VALENT_CONVERSATION_ROW (widget);

  g_assert (VALENT_IS_CONVERSATION_ROW (self));

  gtk_popover_popup (GTK_POPOVER (self->context_menu));
}

/*
 * ValentConversationRow
 */
static GtkWidget *
attachment_list_create (gpointer item,
                         gpointer user_data)
{
  ValentMessageAttachment *attachment = VALENT_MESSAGE_ATTACHMENT (item);
  GtkWidget *row;
  GtkWidget *image;
  GIcon *preview;
  GFile *file;
  g_autofree char *filename = NULL;

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "activatable", TRUE,
                      "selectable",  FALSE,
                      NULL);
  gtk_widget_add_css_class (row, "card");

  preview = valent_message_attachment_get_preview (attachment);
  file = valent_message_attachment_get_file (attachment);
  if (file != NULL)
    filename = g_file_get_basename (file);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "gicon",        preview,
                        "pixel-size",   100,
                        "overflow",     GTK_OVERFLOW_HIDDEN,
                        "tooltip-text", filename,
                        NULL);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), image);

  return row;
}

static void
valent_conversation_row_sync (ValentConversationRow *self)
{
  ValentMessageBox box;
  const char *sender = NULL;
  const char *body = NULL;
  GListModel *attachments;

  g_return_if_fail (VALENT_IS_CONVERSATION_ROW (self));

  /* Reset the row
   */
  gtk_widget_set_visible (self->avatar, FALSE);
  gtk_widget_set_visible (self->sender_label, FALSE);
  gtk_widget_set_visible (self->attachment_list, FALSE);
  gtk_widget_set_visible (self->summary_label, FALSE);
  gtk_widget_set_visible (self->body_label, FALSE);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->attachment_list),
                           NULL, NULL, NULL, NULL);
  self->incoming = FALSE;

  if (self->message == NULL)
    return;

  /* Sent/Received style
   */
  box = valent_message_get_box (self->message);
  if (box == VALENT_MESSAGE_BOX_INBOX)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self), "valent-message-inbox");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "valent-message-outbox");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "valent-message-sent");
    }
  else if (box == VALENT_MESSAGE_BOX_SENT)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self), "valent-message-sent");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "valent-message-inbox");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "valent-message-outbox");
    }
  else if (box == VALENT_MESSAGE_BOX_OUTBOX)
    {
      gtk_widget_add_css_class (GTK_WIDGET (self), "valent-message-outbox");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "valent-message-inbox");
      gtk_widget_remove_css_class (GTK_WIDGET (self), "valent-message-sent");
    }

  self->incoming = (box == VALENT_MESSAGE_BOX_INBOX);
  if (self->incoming)
    {
      gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_START);
      gtk_widget_set_visible (self->avatar, TRUE);
      g_object_set (self->date_label, "xalign", 0.0, NULL);
    }
  else
    {
      gtk_widget_set_halign (GTK_WIDGET (self), GTK_ALIGN_END);
      g_object_set (self->date_label, "xalign", 1.0, NULL);
    }

  /* Attachments
   */
  attachments = valent_message_get_attachments (self->message);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->attachment_list),
                           attachments,
                           attachment_list_create,
                           NULL, NULL);

  if (attachments != NULL)
    {
      gtk_widget_set_visible (self->attachment_list,
                              g_list_model_get_n_items (attachments) > 0);
    }

  /* Sender
   */
  if (self->incoming)
    {
      sender = valent_message_get_sender (self->message);
      if (self->contact != NULL)
        sender = e_contact_get_const (self->contact, E_CONTACT_FULL_NAME);
    }

  gtk_widget_set_visible (self->sender_label, FALSE /* self->incoming */);
  gtk_label_set_label (GTK_LABEL (self->sender_label), sender);

  /* Summary & Body (Message Bubble)
   */
  body = valent_message_get_text (self->message);
  if (body != NULL && *body != '\0')
    {
      g_autofree char *markup = NULL;

      markup = valent_string_to_markup (body);
      gtk_label_set_label (GTK_LABEL (self->body_label), markup);
      gtk_widget_set_visible (self->body_label, TRUE);
    }

  if (gtk_widget_get_visible (self->summary_label) ||
      gtk_widget_get_visible (self->body_label))
    gtk_widget_set_visible (self->message_bubble, TRUE);
  else
    gtk_widget_set_visible (self->message_bubble, FALSE);
}

/*
 * GObject
 */
static void
valent_conversation_row_finalize (GObject *object)
{
  ValentConversationRow *self = VALENT_CONVERSATION_ROW (object);

  g_clear_object (&self->contact);

  if (self->message)
    {
      g_signal_handlers_disconnect_by_data (self->message, self);
      g_clear_object (&self->message);
    }

  G_OBJECT_CLASS (valent_conversation_row_parent_class)->finalize (object);
}

static void
valent_conversation_row_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentConversationRow *self = VALENT_CONVERSATION_ROW (object);

  switch ((ValentConversationRowProperty)prop_id)
    {
    case PROP_CONTACT:
      g_value_set_object (value, self->contact);
      break;

    case PROP_DATE:
      g_value_set_int64 (value, valent_conversation_row_get_date (self));
      break;

    case PROP_MESSAGE:
      g_value_set_object (value, self->message);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_conversation_row_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentConversationRow *self = VALENT_CONVERSATION_ROW (object);

  switch ((ValentConversationRowProperty)prop_id)
    {
    case PROP_CONTACT:
      valent_conversation_row_set_contact (self, g_value_get_object (value));
      break;

    case PROP_MESSAGE:
      valent_conversation_row_set_message (self, g_value_get_object (value));
      break;

    case PROP_DATE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_conversation_row_class_init (ValentConversationRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_conversation_row_finalize;
  object_class->get_property = valent_conversation_row_get_property;
  object_class->set_property = valent_conversation_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-conversation-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, layout);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, sender_label);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, attachment_list);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, message_bubble);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, summary_label);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, body_label);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, date_label);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationRow, context_menu);
  gtk_widget_class_bind_template_callback (widget_class, on_activate_link);
  gtk_widget_class_bind_template_callback (widget_class, on_menu_popup);
  gtk_widget_class_bind_template_callback (widget_class, valent_contact_to_paintable);

  /**
   * ValentConversationRow|clipboard.copy:
   *
   * Copies the message text to the clipboard.
   */
  gtk_widget_class_install_action (widget_class, "clipboard.copy", NULL, clipboard_copy_action);

  /**
   * ValentConversationRow|menu.popup:
   *
   * Opens the context menu.
   */
  gtk_widget_class_install_action (widget_class, "menu.popup", NULL, menu_popup_action);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_F10, GDK_SHIFT_MASK,
                                       "menu.popup",
                                       NULL);
  gtk_widget_class_add_binding_action (widget_class,
                                       GDK_KEY_Menu, 0,
                                       "menu.popup",
                                       NULL);

  /**
   * ValentConversationRow:contact:
   *
   * The `EContact` that sent this message.
   */
  properties [PROP_CONTACT] =
    g_param_spec_object ("contact", NULL, NULL,
                         E_TYPE_CONTACT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentConversationRow:date:
   *
   * The timestamp of the message.
   */
  properties [PROP_DATE] =
    g_param_spec_int64 ("date", NULL, NULL,
                        0, G_MAXINT64,
                        0,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentConversationRow:message:
   *
   * The message this row displays.
   */
  properties [PROP_MESSAGE] =
    g_param_spec_object ("message", NULL, NULL,
                          VALENT_TYPE_MESSAGE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  g_type_ensure (VALENT_TYPE_DATE_LABEL);
}

static void
valent_conversation_row_init (ValentConversationRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * valent_conversation_row_new:
 * @message: a `ValentMessage`
 * @contact: a `EContact`
 *
 * Create a new conversation message for @contact and @message.
 *
 * Returns: (transfer full): a `ValentConversationRow`
 */
GtkWidget *
valent_conversation_row_new (ValentMessage *message,
                             EContact      *contact)
{
  return g_object_new (VALENT_TYPE_CONVERSATION_ROW,
                       "contact", contact,
                       "message", message,
                       NULL);
}

/**
 * valent_conversation_row_get_contact:
 * @row: a `ValentConversationRow`
 *
 * Get the contact.
 *
 * Returns: (transfer none) (nullable): a `ValentContact`
 */
EContact *
valent_conversation_row_get_contact (ValentConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONVERSATION_ROW (row), NULL);

  return row->contact;
}

/**
 * valent_conversation_row_set_contact:
 * @row: a `ValentConversationRow`
 * @contact: a `ValentContact`
 *
 * Set or update the contact.
 */
void
valent_conversation_row_set_contact (ValentConversationRow *row,
                                     EContact              *contact)
{
  g_return_if_fail (VALENT_IS_CONVERSATION_ROW (row));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (g_set_object (&row->contact, contact))
    {
      valent_conversation_row_sync (row);
      g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
    }
}

/**
 * valent_conversation_row_get_date:
 * @row: a `ValentConversationRow`
 *
 * Get the timestamp of the message.
 *
 * Returns: a UNIX epoch timestamp
 */
int64_t
valent_conversation_row_get_date (ValentConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONVERSATION_ROW (row), 0);

  if G_UNLIKELY (row->message == NULL)
    return 0;

  return valent_message_get_date (row->message);
}

/**
 * valent_conversation_row_get_message:
 * @row: a `ValentConversationRow`
 *
 * Get the message.
 *
 * Returns: (transfer none): a `ValentMessage`
 */
ValentMessage *
valent_conversation_row_get_message (ValentConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONVERSATION_ROW (row), NULL);

  return row->message;
}

/**
 * valent_conversation_row_set_message:
 * @row: a `ValentConversationRow`
 * @message: a `ValentMessage`
 *
 * Set or update the message.
 */
void
valent_conversation_row_set_message (ValentConversationRow *row,
                                     ValentMessage         *message)
{
  g_return_if_fail (VALENT_IS_CONVERSATION_ROW (row));
  g_return_if_fail (message == NULL || VALENT_IS_MESSAGE (message));

  if (row->message == message)
    return;

  if (row->message != NULL)
    {
      g_signal_handlers_disconnect_by_data (row->message, row);
      g_clear_object (&row->message);
    }

  if (message != NULL)
    {
      row->message = g_object_ref (message);
      g_signal_connect_swapped (row->message,
                                "notify",
                                G_CALLBACK (valent_conversation_row_sync),
                                row);
    }

  valent_conversation_row_sync (row);
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_MESSAGE]);
}

/**
 * valent_conversation_row_is_incoming:
 * @row: a `ValentConversationRow`
 *
 * Update @row based on the current values of `ValentConversation`:message.
 */
gboolean
valent_conversation_row_is_incoming (ValentConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONVERSATION_ROW (row), FALSE);

  return row->incoming;
}

/**
 * valent_conversation_row_show_avatar:
 * @row: a `ValentConversationRow`
 * @visible: Whether to show the avatar
 *
 * Show or hide the contact avatar for @row.
 */
void
valent_conversation_row_show_avatar (ValentConversationRow *row,
                                     gboolean               visible)
{
  g_return_if_fail (VALENT_IS_CONVERSATION_ROW (row));

  if G_LIKELY (gtk_widget_get_visible (row->avatar) == visible)
    return;

  gtk_widget_set_visible (row->avatar, visible);
}

