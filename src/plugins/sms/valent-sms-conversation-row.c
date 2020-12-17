// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-conversation-row"

#include "config.h"

#include <gtk/gtk.h>
#include <pango/pango.h>
#include <libvalent-contacts.h>

#include "valent-contact-avatar.h"
#include "valent-sms-conversation-row.h"
#include "valent-sms-message.h"


struct _ValentSmsConversationRow
{
  GtkListBoxRow     parent_instance;

  ValentSmsMessage *message;
  EContact         *contact;
  unsigned int      incoming : 1;

  GtkWidget        *grid;
  GtkWidget        *avatar;
  GtkWidget        *bubble;
  GtkWidget        *text_label;
};

G_DEFINE_TYPE (ValentSmsConversationRow, valent_sms_conversation_row, GTK_TYPE_LIST_BOX_ROW)


enum {
  PROP_0,
  PROP_CONTACT,
  PROP_DATE,
  PROP_MESSAGE,
  PROP_THREAD_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

static GRegex *url_pattern = NULL;


static char *
linkify_text (const char *text)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *escaped = NULL;
  char *parsed;

  g_return_val_if_fail (text != NULL, NULL);

  if G_UNLIKELY (url_pattern == NULL)
    url_pattern = g_regex_new ("(https?:[/]{0,2})?[^\\s/$.?#]+[.][^\\s]*",
                               G_REGEX_OPTIMIZE,
                               0,
                               &error);

  if (url_pattern == NULL)
    {
      g_debug ("[%s] %s", G_STRFUNC, error->message);
      return NULL;
    }

  escaped = g_markup_escape_text(text, -1);

  parsed = g_regex_replace (url_pattern, escaped, -1, 0,
                            "<a href=\"\\0\">\\0</a>", 0, &error);

  if G_UNLIKELY (parsed == NULL)
    g_warning ("[%s] %s", G_STRFUNC, error->message);

  return parsed;
}

static gboolean
valent_sms_conversation_row_activate_link (GtkLabel   *label,
                                           const char *uri,
                                           gpointer    user_data)
{
  GtkWidget *widget = GTK_WIDGET (label);
  GtkWidget *toplevel = GTK_WIDGET (gtk_widget_get_root (widget));
  g_autofree char *url = NULL;

  /* Only handle links that need to be amended with a scheme */
  if (g_uri_parse_scheme (uri) != NULL)
    return FALSE;

  if (!GTK_IS_WINDOW (toplevel))
    return FALSE;

  url = g_strdup_printf ("https://%s", uri);
  gtk_show_uri (GTK_WINDOW (toplevel), url, GDK_CURRENT_TIME);

  return TRUE;
}

/*
 * GObject
 */
static void
valent_sms_conversation_row_constructed (GObject *object)
{
  ValentSmsConversationRow *self = VALENT_SMS_CONVERSATION_ROW (object);

  if (self->message != NULL)
    valent_sms_conversation_row_update (self);

  G_OBJECT_CLASS (valent_sms_conversation_row_parent_class)->constructed (object);
}

static void
valent_sms_conversation_row_finalize (GObject *object)
{
  ValentSmsConversationRow *self = VALENT_SMS_CONVERSATION_ROW (object);

  g_clear_object (&self->contact);
  g_clear_object (&self->message);

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

    case PROP_THREAD_ID:
      g_value_set_int64 (value, valent_sms_conversation_row_get_thread_id (self));
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

  object_class->constructed = valent_sms_conversation_row_constructed;
  object_class->finalize = valent_sms_conversation_row_finalize;
  object_class->get_property = valent_sms_conversation_row_get_property;
  object_class->set_property = valent_sms_conversation_row_set_property;

  /**
   * ValentSmsConversationRow:contact
   *
   * The #EContact that sent this message.
   */
  properties [PROP_CONTACT] =
    g_param_spec_object ("contact",
                         "Contact",
                         "The contact that sent this message.",
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
    g_param_spec_int64 ("date",
                        "Date",
                        "The timestamp of the message.",
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
    g_param_spec_object ("message",
                         "Message",
                         "The message this row displays.",
                          VALENT_TYPE_SMS_MESSAGE,
                          (G_PARAM_READWRITE |
                           G_PARAM_CONSTRUCT |
                           G_PARAM_EXPLICIT_NOTIFY |
                           G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsConversationRow:thread-id
   *
   * The thread id this message belongs to.
   */
  properties [PROP_THREAD_ID] =
    g_param_spec_string ("thread-id",
                         "Thread ID",
                         "The thread id this message belongs to.",
                         NULL,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_sms_conversation_row_init (ValentSmsConversationRow *self)
{
  GtkStyleContext *style;

  style = gtk_widget_get_style_context (GTK_WIDGET (self));
  gtk_style_context_add_class (style, "valent-sms-conversation-row");

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
  self->avatar = g_object_new (VALENT_TYPE_CONTACT_AVATAR,
                               "height-request", 32,
                               "width-request",  32,
                               "halign",         GTK_ALIGN_START,
                               "valign",         GTK_ALIGN_END,
                               "vexpand",        TRUE,
                               "visible",        FALSE,
                               NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->avatar, 0, 0, 1, 1);

  g_object_bind_property (self,         "contact",
                          self->avatar, "contact",
                          G_BINDING_SYNC_CREATE);

  /* Message Layout */
  self->bubble = g_object_new (GTK_TYPE_GRID, NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->bubble, 1, 0, 1, 1);

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
 * @message: a #ValentSmsMessage
 * @contact: a #EContact
 *
 * Create a new conversation message for @contact and @message.
 *
 * Returns: (transfer full): a #ValentSmsConversationRow
 */
GtkWidget *
valent_sms_conversation_row_new (ValentSmsMessage *message,
                                 EContact         *contact)
{
  return g_object_new (VALENT_TYPE_SMS_CONVERSATION_ROW,
                       "contact", contact,
                       "message", message,
                       NULL);
}

/**
 * valent_sms_conversation_row_get_contact:
 * @row: a #ValentSmsConversationRow
 *
 * Get the contact.
 *
 * Returns: (transfer none) (nullable): a #ValentContact
 */
EContact *
valent_sms_conversation_row_get_contact (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), NULL);

  return row->contact;
}

/**
 * valent_sms_conversation_row_set_contact:
 * @row: a #ValentSmsConversationRow
 * @contact: a #ValentContact
 *
 * Set or update the contact.
 */
void
valent_sms_conversation_row_set_contact (ValentSmsConversationRow *row,
                                         EContact                 *contact)
{
  g_return_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (g_set_object (&row->contact, contact))
    {
      valent_sms_conversation_row_update (row);
      g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
    }
}

/**
 * valent_sms_conversation_row_get_date:
 * @row: a #ValentSmsConversationRow
 *
 * Get the timestamp of the message.
 *
 * Returns: a UNIX epoch timestamp
 */
gint64
valent_sms_conversation_row_get_date (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), 0);

  if G_UNLIKELY (row->message == NULL)
    return 0;

  return valent_sms_message_get_date (row->message);
}

/**
 * valent_sms_conversation_row_get_thread_id:
 * @row: a #ValentSmsConversationRow
 *
 * Get the thread_id of the message.
 *
 * Returns: a thread id
 */
gint64
valent_sms_conversation_row_get_thread_id (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), 0);

  if G_UNLIKELY (row->message == NULL)
    return 0;

  return valent_sms_message_get_thread_id (row->message);
}

/**
 * valent_sms_conversation_row_get_message:
 * @row: a #ValentSmsConversationRow
 *
 * Get the message.
 *
 * Returns: (transfer none): a #ValentSmsMessage
 */
ValentSmsMessage *
valent_sms_conversation_row_get_message (ValentSmsConversationRow *row)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row), NULL);

  return row->message;
}

/**
 * valent_sms_conversation_row_set_message:
 * @row: a #ValentSmsConversationRow
 * @message: a #ValentMessage
 *
 * Set or update the message.
 */
void
valent_sms_conversation_row_set_message (ValentSmsConversationRow *row,
                                         ValentSmsMessage         *message)
{
  g_return_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row));
  g_return_if_fail (message == NULL || VALENT_IS_SMS_MESSAGE (message));

  if (g_set_object (&row->message, message))
    {
      valent_sms_conversation_row_update (row);
      g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_MESSAGE]);
    }
}

/**
 * valent_sms_conversation_row_is_incoming:
 * @row: a #ValentSmsConversationRow
 *
 * Update @row based on the current values of #ValentSmsConversation:message.
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
 * @row: a #ValentSmsConversationRow
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
 * @row: a #ValentSmsConversationRow
 *
 * Update @row based on the current values of #ValentSmsConversation:message.
 */
void
valent_sms_conversation_row_update (ValentSmsConversationRow *row)
{
  // TODO margins, avatar, date, etc
  GtkStyleContext *style;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION_ROW (row));

  // text
  if (row->message == NULL)
    return;

  row->incoming = valent_sms_message_get_box (row->message) == VALENT_SMS_MESSAGE_BOX_INBOX;

  gtk_widget_set_visible (row->avatar, row->incoming);

  // margin (6px) + avatar (32px) + spacing (6px)
  // 2 x above for outgoing
  if (row->incoming)
    {
      gtk_widget_set_halign (row->grid, GTK_ALIGN_START);
      gtk_widget_set_margin_end (row->grid, 44);
      gtk_widget_set_margin_start (row->grid, 6);
    }
  else
    {
      gtk_widget_set_halign (row->grid, GTK_ALIGN_END);
      gtk_widget_set_margin_end (row->grid, 6);
      gtk_widget_set_margin_start (row->grid, 88);
    }

  /* Message Body */
  if (row->message != NULL)
    {
      const char *text;
      g_autofree char *label = NULL;

      text = valent_sms_message_get_text (row->message);
      //label = g_markup_escape_text (text, -1);
      label = linkify_text (text);
      g_object_set (row->text_label, "label", label, NULL);
    }

  /* Incoming/Outgoing style */
  style = gtk_widget_get_style_context (row->bubble);

  if (row->incoming)
    {
      gtk_style_context_remove_class (style, "valent-sms-outgoing");
      gtk_style_context_add_class (style, "valent-sms-incoming");
      gtk_widget_set_halign (GTK_WIDGET (row), GTK_ALIGN_START);
    }
  else
    {
      gtk_style_context_remove_class (style, "valent-sms-incoming");
      gtk_style_context_add_class (style, "valent-sms-outgoing");
      gtk_widget_set_halign (GTK_WIDGET (row), GTK_ALIGN_END);
    }
}

