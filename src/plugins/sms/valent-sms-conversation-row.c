// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-conversation-row"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

#include "valent-message.h"
#include "valent-sms-conversation-row.h"
#include "valent-sms-utils.h"


struct _ValentSmsConversationRow
{
  GtkListBoxRow  parent_instance;

  ValentMessage *message;
  EContact      *contact;
  unsigned int   incoming : 1;

  GtkWidget     *grid;
  GtkWidget     *avatar;
  GtkWidget     *bubble;
  GtkWidget     *text_label;
};

G_DEFINE_FINAL_TYPE (ValentSmsConversationRow, valent_sms_conversation_row, GTK_TYPE_LIST_BOX_ROW)


enum {
  PROP_0,
  PROP_CONTACT,
  PROP_DATE,
  PROP_MESSAGE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/* LCOV_EXCL_START */
static gboolean
valent_sms_conversation_row_activate_link (GtkLabel   *label,
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
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_sms_conversation_row_finalize (GObject *object)
{
  ValentSmsConversationRow *self = VALENT_SMS_CONVERSATION_ROW (object);

  g_clear_object (&self->contact);

  if (self->message)
    {
      g_signal_handlers_disconnect_by_data (self->message, self);
      g_clear_object (&self->message);
    }

  G_OBJECT_CLASS (valent_sms_conversation_row_parent_class)->finalize (object);
}

static void
valent_sms_conversation_row_get_property (GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
  ValentSmsConversationRow *self = VALENT_SMS_CONVERSATION_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTACT:
      g_value_set_object (value, self->contact);
      break;

    case PROP_DATE:
      g_value_set_int64 (value, valent_sms_conversation_row_get_date (self));
      break;

    case PROP_MESSAGE:
      g_value_set_object (value, self->message);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_conversation_row_set_property (GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
  ValentSmsConversationRow *self = VALENT_SMS_CONVERSATION_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTACT:
      valent_sms_conversation_row_set_contact (self, g_value_get_object (value));
      break;

    case PROP_MESSAGE:
      valent_sms_conversation_row_set_message (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_conversation_row_class_init (ValentSmsConversationRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_sms_conversation_row_finalize;
  object_class->get_property = valent_sms_conversation_row_get_property;
  object_class->set_property = valent_sms_conversation_row_set_property;

  /**
   * ValentSmsConversationRow:contact
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
   * ValentSmsConversationRow:date
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
   * ValentSmsConversationRow:message
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

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_sms_conversation_row_init (ValentSmsConversationRow *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self), "valent-sms-conversation-row");

  self->grid = g_object_new (GTK_TYPE_GRID,
                             "can-focus",      FALSE,
                             "column-spacing", 6,
                             "hexpand",        TRUE,
                             "margin-start",   6,
                             "margin-end",     6,
                             "margin-top",     6,
                             "margin-bottom",  6,
                             NULL);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (self), self->grid);

  /* Contact Avatar */
  self->avatar = g_object_new (ADW_TYPE_AVATAR,
                               "size",    32,
                               "halign",  GTK_ALIGN_START,
                               "valign",  GTK_ALIGN_END,
                               "vexpand", TRUE,
                               "visible", FALSE,
                               NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->avatar, 0, 0, 1, 1);

  /* Message Layout */
  self->bubble = g_object_new (GTK_TYPE_GRID, NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->bubble, 1, 0, 1, 1);

  /* Message Text */
  self->text_label = g_object_new (GTK_TYPE_LABEL,
                                   "halign",     GTK_ALIGN_START,
                                   "use-markup", TRUE,
                                   "selectable", TRUE,
                                   "wrap",       TRUE,
                                   "wrap-mode",  PANGO_WRAP_WORD_CHAR,
                                   "xalign",     0.0,
                                   NULL);
  gtk_widget_set_can_focus (self->text_label, FALSE);
  gtk_grid_attach (GTK_GRID (self->bubble), self->text_label, 0, 0, 1, 1);

  /* Catch activate-link to fixup URIs without a scheme */
  g_signal_connect (self->text_label,
                    "activate-link",
                    G_CALLBACK (valent_sms_conversation_row_activate_link),
                    NULL);
}

/**
 * valent_sms_conversation_row_new:
 * @message: a `ValentMessage`
 * @contact: a `EContact`
 *
 * Create a new conversation message for @contact and @message.
 *
 * Returns: (transfer full): a `ValentSmsConversationRow`
 */
GtkWidget *
valent_sms_conversation_row_new (ValentMessage *message,
                                 EContact      *contact)
{
  return g_object_new (VALENT_TYPE_SMS_CONVERSATION_ROW,
                       "contact", contact,
                       "message", message,
                       NULL);
}

/**
 * valent_sms_conversation_row_get_contact:
 * @row: a `ValentSmsConversationRow`
 *
 * Get the contact.
 *
 * Returns: (transfer none) (nullable): a `ValentContact`
 */
EContact *
valent_sms_conversation_row_get_contact (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), NULL);

  return row->contact;
}

/**
 * valent_sms_conversation_row_set_contact:
 * @row: a `ValentSmsConversationRow`
 * @contact: a `ValentContact`
 *
 * Set or update the contact.
 */
void
valent_sms_conversation_row_set_contact (ValentSmsConversationRow *row,
                                         EContact                 *contact)
{
  g_return_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (!g_set_object (&row->contact, contact))
    return;

  if (row->contact != NULL)
    valent_sms_avatar_from_contact (ADW_AVATAR (row->avatar), contact);

  valent_sms_conversation_row_update (row);
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
}

/**
 * valent_sms_conversation_row_get_date:
 * @row: a `ValentSmsConversationRow`
 *
 * Get the timestamp of the message.
 *
 * Returns: a UNIX epoch timestamp
 */
int64_t
valent_sms_conversation_row_get_date (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), 0);

  if G_UNLIKELY (row->message == NULL)
    return 0;

  return valent_message_get_date (row->message);
}

/**
 * valent_sms_conversation_row_get_id:
 * @row: a `ValentSmsConversationRow`
 *
 * Get the ID of the message.
 *
 * Returns: a message id
 */
int64_t
valent_sms_conversation_row_get_id (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), 0);

  if G_UNLIKELY (row->message == NULL)
    return 0;

  return valent_message_get_id (row->message);
}

/**
 * valent_sms_conversation_row_get_message:
 * @row: a `ValentSmsConversationRow`
 *
 * Get the message.
 *
 * Returns: (transfer none): a `ValentMessage`
 */
ValentMessage *
valent_sms_conversation_row_get_message (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), NULL);

  return row->message;
}

/**
 * valent_sms_conversation_row_set_message:
 * @row: a `ValentSmsConversationRow`
 * @message: a `ValentMessage`
 *
 * Set or update the message.
 */
void
valent_sms_conversation_row_set_message (ValentSmsConversationRow *row,
                                         ValentMessage            *message)
{
  g_return_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row));
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
                                G_CALLBACK (valent_sms_conversation_row_update),
                                row);

      valent_sms_conversation_row_update (row);
      g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_MESSAGE]);
    }
}

/**
 * valent_sms_conversation_row_is_incoming:
 * @row: a `ValentSmsConversationRow`
 *
 * Update @row based on the current values of `ValentSmsConversation`:message.
 */
gboolean
valent_sms_conversation_row_is_incoming (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), FALSE);

  if (row->message == NULL)
      return FALSE;

  return row->incoming;
}

/**
 * valent_sms_conversation_row_show_avatar:
 * @row: a `ValentSmsConversationRow`
 * @visible: Whether to show the avatar
 *
 * Show or hide the contact avatar for @row, updating the margins accordingly.
 */
void
valent_sms_conversation_row_show_avatar (ValentSmsConversationRow *row,
                                         gboolean                  visible)
{
  g_return_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row));

  if G_LIKELY (gtk_widget_get_visible (row->avatar) == visible)
    return;

  if (visible)
    {
      gtk_widget_set_margin_start (row->bubble, 6);
      gtk_widget_set_margin_bottom (row->bubble, 6);
    }
  else
    {
      gtk_widget_set_margin_start (row->bubble, 44);
      gtk_widget_set_margin_bottom (row->bubble, 0);
    }

  gtk_widget_set_visible (row->avatar, visible);
}

/**
 * valent_sms_conversation_row_update:
 * @row: a `ValentSmsConversationRow`
 *
 * Update @row based on the current values of `ValentSmsConversation`:message.
 */
void
valent_sms_conversation_row_update (ValentSmsConversationRow *row)
{
  const char *text;
  g_autofree char *label = NULL;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row));

  if (row->message == NULL)
    return;

  /* The row style consists of the alignment and the CSS
   *
   * The row margin opposite the avatar is chosen to balance the row. Outgoing
   * messages don't show avatars, so get double the margin (88px):
   *
   *     44px (margin) = 6px (margin) + 32px (avatar) + 6px (spacing)
   *
   * The CSS classes determine the chat bubble style and color.
   */
  row->incoming = valent_message_get_box (row->message) == VALENT_MESSAGE_BOX_INBOX;
  if (row->incoming)
    {
      gtk_widget_set_halign (row->grid, GTK_ALIGN_START);
      gtk_widget_set_margin_end (row->grid, 44);
      gtk_widget_set_margin_start (row->grid, 6);

      gtk_widget_remove_css_class (row->bubble, "valent-sms-outgoing");
      gtk_widget_add_css_class (row->bubble, "valent-sms-incoming");
      gtk_widget_set_halign (GTK_WIDGET (row), GTK_ALIGN_START);
      gtk_widget_set_visible (row->avatar, TRUE);
    }
  else
    {
      gtk_widget_set_halign (row->grid, GTK_ALIGN_END);
      gtk_widget_set_margin_end (row->grid, 6);
      gtk_widget_set_margin_start (row->grid, 88);

      gtk_widget_remove_css_class (row->bubble, "valent-sms-incoming");
      gtk_widget_add_css_class (row->bubble, "valent-sms-outgoing");
      gtk_widget_set_halign (GTK_WIDGET (row), GTK_ALIGN_END);
      gtk_widget_set_visible (row->avatar, FALSE);
    }

  /* Text content
   */
  text = valent_message_get_text (row->message);
  label = valent_string_to_markup (text);
  gtk_label_set_label (GTK_LABEL (row->text_label), label);
}

