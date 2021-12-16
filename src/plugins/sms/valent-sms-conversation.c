// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-conversation"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-contacts.h>

#include "valent-date-label.h"
#include "valent-sms-conversation.h"
#include "valent-sms-conversation-row.h"
#include "valent-sms-message.h"
#include "valent-sms-store.h"


struct _ValentSmsConversation
{
  GtkWidget           parent_instance;

  /* Template Widgets */
  GtkListBox         *message_list;
  GtkEntry           *message_entry;
  GtkScrolledWindow  *conversation_view;
  GtkListBoxRow      *pending;

  /* Population */
  guint               populate_id;
  double              pos;
  GtkAdjustment      *vadj;

  /* Thread Resources */
  gint64              thread_id;
  ValentSmsStore     *message_store;
  GQueue             *messages;
  ValentContactStore *contact_store;
  GHashTable         *participants;

  char               *title;
  char               *subtitle;
};

G_DEFINE_TYPE (ValentSmsConversation, valent_sms_conversation, GTK_TYPE_WIDGET)

enum {
  PROP_0,
  PROP_CONTACT_STORE,
  PROP_MESSAGE_STORE,
  PROP_THREAD_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  SEND_MESSAGE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static void
valent_sms_conversation_jump_to_row (ValentSmsConversation *self,
                                     GtkWidget             *widget)
{
  GtkWidget *viewport;
  double upper, page_size;
  double x, y;

  /* Get the scrolled window state */
  upper = gtk_adjustment_get_upper (self->vadj);
  page_size = gtk_adjustment_get_page_size (self->vadj);

  /* Get the widget's position in the window */
  viewport = gtk_scrolled_window_get_child (self->conversation_view);
  gtk_widget_translate_coordinates (widget, viewport, 0, 0, &x, &y);

  /* Scroll to the position */
  gtk_adjustment_set_value (self->vadj, CLAMP (y, page_size, upper));
  gtk_widget_grab_focus (widget);
}

/**
 * valent_sms_conversation_insert_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentSmsMessage
 * @position: position to insert the widget
 *
 * Create a new message row for @message and insert it into the message list at
 * @position.
 *
 * Returns: (transfer none): a #GtkWidget
 */
static GtkWidget *
valent_sms_conversation_insert_message (ValentSmsConversation *self,
                                        ValentSmsMessage      *message,
                                        gint                   position)
{
  GtkWidget *row;
  EContact *contact = NULL;
  const char *address;

  g_assert (VALENT_IS_SMS_CONVERSATION (self));
  g_assert (VALENT_IS_SMS_MESSAGE (message));

  if ((address = valent_sms_message_get_sender (message)) != NULL)
    contact = g_hash_table_lookup (self->participants, address);
  else if (valent_sms_message_get_box (message) == VALENT_SMS_MESSAGE_BOX_INBOX)
    g_warning ("Message missing address");

  if (contact == NULL && address != NULL)
    {
      contact = valent_contact_store_dup_for_phone (self->contact_store, address);
      g_hash_table_insert (self->participants, g_strdup (address), contact);
    }

  /* Insert the row into the message list */
  row = g_object_new (VALENT_TYPE_SMS_CONVERSATION_ROW,
                      "contact",     contact,
                      "message",     message,
                      "activatable", FALSE,
                      "selectable",  FALSE,
                      NULL);

  gtk_list_box_insert (self->message_list, row, position);
  gtk_list_box_invalidate_headers (self->message_list);

  return row;
}

static void
message_list_header_func (GtkListBoxRow *row,
                          GtkListBoxRow *before,
                          gpointer       user_data)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (user_data);
  ValentSmsConversationRow *mrow = VALENT_SMS_CONVERSATION_ROW (row);
  ValentSmsConversationRow *brow = VALENT_SMS_CONVERSATION_ROW (before);
  gint64 row_date, before_date;
  gboolean row_incoming, before_incoming;

  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (before == NULL || GTK_IS_LIST_BOX_ROW (before));

  /* TODO Skip pending */
  if G_UNLIKELY (row == self->pending)
    return;

  row_incoming = valent_sms_conversation_row_is_incoming (mrow);

  /* If this is the first row and it's incoming, show the avatar */
  if (before == NULL)
    {
      if (row_incoming)
        valent_sms_conversation_row_show_avatar (mrow, TRUE);
      return;
    }

  /* Date header */
  before_incoming = valent_sms_conversation_row_is_incoming (brow);
  before_date = valent_sms_conversation_row_get_date (brow);
  row_date = valent_sms_conversation_row_get_date (mrow);

  /* If it's been more than an hour between messages, show a date label */
  if (row_date - before_date > G_TIME_SPAN_HOUR / 1000)
    {
      /* Show a human-readable time span label */
      if G_UNLIKELY (gtk_list_box_row_get_header (row) == NULL)
        {
          GtkWidget *header;

          header = valent_date_label_new (row_date);
          gtk_style_context_add_class (gtk_widget_get_style_context (header),
                                       "dim-label");
          gtk_list_box_row_set_header (row, header);

          /* If the row's message is incoming, show the avatar also */
          if (row_incoming)
            valent_sms_conversation_row_show_avatar (mrow, row_incoming);
        }
    }
  else if (row_incoming)
    {
      valent_sms_conversation_row_show_avatar (mrow, TRUE);

      /* If the previous row was incoming, hide its avatar */
      if (before_incoming)
        valent_sms_conversation_row_show_avatar (mrow, FALSE);
    }
}

/**
 * valent_conversation_append_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentSmsMessage
 *
 * Append a message to the conversation.
 *
 * Returns: (transfer none): a #GtkWidget
 */
static GtkWidget *
valent_sms_conversation_append_message (ValentSmsConversation *conversation,
                                        ValentSmsMessage         *message)
{
  g_assert (VALENT_IS_SMS_CONVERSATION (conversation));
  g_assert (VALENT_IS_SMS_MESSAGE (message));

  return valent_sms_conversation_insert_message (conversation, message, -1);
}

/**
 * valent_conversation_prepend_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentSmsMessage
 *
 * Prepend a message to the conversation.
 *
 * Returns: (transfer none): a #GtkWidget
 */
static GtkWidget *
valent_sms_conversation_prepend_message (ValentSmsConversation *conversation,
                                         ValentSmsMessage         *message)
{
  g_assert (VALENT_IS_SMS_CONVERSATION (conversation));
  g_assert (VALENT_IS_SMS_MESSAGE (message));

  return valent_sms_conversation_insert_message (conversation, message, 0);
}

/**
 * valent_conversation_remove_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentSmsMessage
 *
 * Remove a message from the conversation.
 */
static void
valent_sms_conversation_remove_message (ValentSmsConversation *conversation,
                                        gint64                 message_id)
{
  GtkWidget *row;

  g_assert (VALENT_IS_SMS_CONVERSATION (conversation));
  g_assert (message_id > 0);

  for (row = gtk_widget_get_first_child (GTK_WIDGET (conversation->message_list));
       row != NULL;
       row = gtk_widget_get_next_sibling (row))
    {
      if (valent_sms_conversation_row_get_date (VALENT_SMS_CONVERSATION_ROW (row)) == message_id)
        {
          gtk_list_box_remove (conversation->message_list, row);
          break;
        }
    }
}

/**
 * valent_sms_conversation_populate_reverse:
 * @conversation: a #ValentSmsConversation
 *
 * Populate messages in reverse, in chunks of series.
 */
static void
valent_sms_conversation_populate_reverse (ValentSmsConversation *self)
{
  gint64 prev_type = -1;

  if (self->messages == NULL)
    return;

  while (self->messages->tail != NULL)
    {
      ValentSmsMessage *message;
      ValentSmsMessageBox box;

      message = g_queue_peek_tail (self->messages);
      box = valent_sms_message_get_box (message);

      /* TODO: unknown message types */
      if (box != VALENT_SMS_MESSAGE_BOX_INBOX &&
          box != VALENT_SMS_MESSAGE_BOX_SENT)
        {
          g_warning ("Unknown message type '%li'; discarding", (gint64)box);
          g_queue_pop_tail (self->messages);
          continue;
        }

      if (prev_type == -1)
        prev_type = box;

      /* Break if the direction has changed since the last iteration */
      if (prev_type != box)
        break;

      valent_sms_conversation_prepend_message (self, message);
      g_queue_pop_tail (self->messages);
   }
}

static void
on_message_added (ValentSmsStore        *store,
                  gint64                 thread_id,
                  ValentSmsMessage      *message,
                  ValentSmsConversation *conversation)
{
  if (conversation->thread_id == thread_id)
    valent_sms_conversation_append_message (conversation, message);
}

static void
on_message_removed (ValentSmsStore        *store,
                    gint64                 thread_id,
                    gint64                 message_id,
                    ValentSmsConversation *conversation)
{
  if (conversation->thread_id == thread_id)
    valent_sms_conversation_remove_message (conversation, message_id);
}

/*
 * Message Entry Callbacks
 */
static void
on_entry_activated (GtkEntry              *entry,
                    ValentSmsConversation *conversation)
{
  valent_sms_conversation_send_message (conversation);
}

static void
on_entry_icon_release (GtkEntry              *entry,
                       GtkEntryIconPosition   icon_pos,
                       ValentSmsConversation *conversation)
{
  valent_sms_conversation_send_message (conversation);
}

static void
on_entry_changed (GtkEntry              *entry,
                  ValentSmsConversation *conversation)
{
  gboolean has_message;
  const char *text;

  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  has_message = (g_utf8_strlen (text, -1) > 0);

  gtk_entry_set_icon_sensitive (entry, GTK_ENTRY_ICON_SECONDARY, has_message);
}

/*
 * Auto-scroll
 */
static void push_populate_reverse (ValentSmsConversation *self);

static gboolean
populate_messages (gpointer user_data)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (user_data);

  if (gtk_adjustment_get_upper (self->vadj) == 0)
    return G_SOURCE_CONTINUE;

  valent_sms_conversation_populate_reverse (self);
  self->populate_id = 0;

  push_populate_reverse (self);

  return G_SOURCE_REMOVE;
}

static void
push_populate_reverse (ValentSmsConversation *self)
{
  double upper, page_size;

  upper = gtk_adjustment_get_upper (self->vadj);
  page_size = gtk_adjustment_get_page_size (self->vadj);

  if (self->populate_id == 0 && upper <= page_size)
    self->populate_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                         populate_messages,
                                         self,
                                         NULL);
}

static void
on_edge_reached (GtkScrolledWindow     *scrolled_window,
                 GtkPositionType        pos,
                 ValentSmsConversation *self)
{
  if (pos == GTK_POS_TOP)
    valent_sms_conversation_populate_reverse (self);

  if (pos == GTK_POS_BOTTOM)
    self->pos = 0;
}

static void
on_scroll_changed (GtkAdjustment         *adjustment,
                   ValentSmsConversation *self)
{
  double upper, page_size;

  if G_UNLIKELY (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  upper = gtk_adjustment_get_upper (self->vadj);
  page_size = gtk_adjustment_get_page_size (self->vadj);

  /* We were asked to hold a position */
  if (self->pos > 0)
    gtk_adjustment_set_value (self->vadj, upper - self->pos);

  /* A message was added, so scroll to the bottom */
  else
    gtk_adjustment_set_value (self->vadj, upper - page_size);

  push_populate_reverse (self);
}

static void
on_scroll_position (GtkAdjustment         *adjustment,
                    ValentSmsConversation *self)
{
  double upper;
  double value;

  upper = gtk_adjustment_get_upper (adjustment);
  value = gtk_adjustment_get_value (adjustment);

  self->pos = upper - value;
}

static void
on_realize (GtkWidget             *widget,
            ValentSmsConversation *self)
{
  self->populate_id = g_idle_add_full (G_PRIORITY_LOW,
                                       populate_messages,
                                       self,
                                       NULL);
}

/*
 * GObject
 */
static void
valent_sms_conversation_constructed (GObject *object)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (object);

  /* Setup thread messages */
  if (self->message_store)
    {
      g_signal_connect (self->message_store,
                        "message-added",
                        G_CALLBACK (on_message_added),
                        self);

      g_signal_connect (self->message_store,
                        "message-removed",
                        G_CALLBACK (on_message_removed),
                        self);

      if (self->thread_id)
        self->messages = valent_sms_store_dup_thread (self->message_store,
                                                      self->thread_id);
    }

  /* Watch the scroll position */
  g_signal_connect (self->vadj,
                    "changed",
                    G_CALLBACK (on_scroll_changed),
                    self);
  g_signal_connect (self->vadj,
                    "value-changed",
                    G_CALLBACK (on_scroll_position),
                    self);

  G_OBJECT_CLASS (valent_sms_conversation_parent_class)->constructed (object);
}

static void
valent_sms_conversation_dispose (GObject *object)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (object);

  if (GTK_IS_WIDGET (self->conversation_view))
    gtk_widget_unparent (GTK_WIDGET (self->conversation_view));

  if (GTK_IS_WIDGET (self->message_entry))
    gtk_widget_unparent (GTK_WIDGET (self->message_entry));

  G_OBJECT_CLASS (valent_sms_conversation_parent_class)->dispose (object);
}

static void
valent_sms_conversation_finalize (GObject *object)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (object);

  if (self->message_store)
    {
      g_signal_handlers_disconnect_by_func (self->message_store,
                                            on_message_added,
                                            self);
      g_signal_handlers_disconnect_by_func (self->message_store,
                                            on_message_removed,
                                            self);
      g_clear_object (&self->message_store);
      g_clear_pointer (&self->messages, g_queue_free);
    }

  if (self->contact_store)
    {
      g_clear_object (&self->contact_store);
      g_clear_pointer (&self->participants, g_hash_table_unref);
    }

  G_OBJECT_CLASS (valent_sms_conversation_parent_class)->finalize (object);
}

static void
valent_sms_conversation_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (object);

  switch (prop_id)
    {
    case PROP_CONTACT_STORE:
      g_value_set_object (value, self->contact_store);
      break;

    case PROP_MESSAGE_STORE:
      g_value_set_object (value, self->message_store);
      break;

    case PROP_THREAD_ID:
      g_value_set_int64 (value, self->thread_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_conversation_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (object);

  switch (prop_id)
    {
    case PROP_CONTACT_STORE:
      self->contact_store = g_value_dup_object (value);
      break;

    case PROP_MESSAGE_STORE:
      self->message_store = g_value_dup_object (value);
      break;

    case PROP_THREAD_ID:
      self->thread_id = g_value_get_int64 (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_conversation_class_init (ValentSmsConversationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_sms_conversation_constructed;
  object_class->dispose = valent_sms_conversation_dispose;
  object_class->finalize = valent_sms_conversation_finalize;
  object_class->get_property = valent_sms_conversation_get_property;
  object_class->set_property = valent_sms_conversation_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sms/valent-sms-conversation.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, message_list);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, message_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, conversation_view);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, pending);

  gtk_widget_class_bind_template_callback (widget_class, on_entry_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_icon_release);
  gtk_widget_class_bind_template_callback (widget_class, on_edge_reached);
  gtk_widget_class_bind_template_callback (widget_class, on_realize);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_GRID_LAYOUT);

  /**
   * ValentSmsConversation:contact-store:
   *
   * The #ValentContactStore providing #EContact objects for the conversation.
   */
  properties [PROP_CONTACT_STORE] =
    g_param_spec_object ("contact-store",
                         "Contact Store",
                         "Contact store providing contact information",
                         VALENT_TYPE_CONTACT_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsConversation:message-store:
   *
   * The #ValentSmsStore providing #ValentSmsMessage objects for the
   * conversation.
   */
  properties [PROP_MESSAGE_STORE] =
    g_param_spec_object ("message-store",
                         "Message Store",
                         "The SMS message store for this conversation.",
                         VALENT_TYPE_SMS_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsConversation:thread-id:
   *
   * The thread ID of the conversation.
   */
  properties [PROP_THREAD_ID] =
    g_param_spec_int64 ("thread-id",
                        "Thread ID",
                        "The thread ID of the conversation",
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentSmsConversation::send-message:
   * @conversation: a #ValentSmsConversation
   * @participants: a #GList
   * @message: a message
   *
   * The #ValentSmsConversation::send-message signal is emitted when a user is
   * sending an outgoing message.
   */
  signals [SEND_MESSAGE] =
    g_signal_new ("send-message",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_BOOLEAN, 2, G_TYPE_POINTER, VALENT_TYPE_SMS_MESSAGE);
}

static void
valent_sms_conversation_init (ValentSmsConversation *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->vadj = gtk_scrolled_window_get_vadjustment (self->conversation_view);

  gtk_list_box_set_header_func (self->message_list,
                                message_list_header_func,
                                self, NULL);

  self->participants = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              g_free,     g_object_unref);
}

GtkWidget *
valent_sms_conversation_new (ValentContactStore *contacts,
                             ValentSmsStore     *messages)
{
  return g_object_new (VALENT_TYPE_SMS_CONVERSATION,
                       "contact-store", contacts,
                       "message-store", messages,
                       NULL);
}

/**
 * valent_sms_conversation_send_message:
 * @conversation: a #ValentSmsConversation
 *
 * Emit #ValentSmsConversation::send-message with the current contents of the
 * message entry.
 */
void
valent_sms_conversation_send_message (ValentSmsConversation *self)
{
  g_autoptr (GList) participants = NULL;
  g_autoptr (ValentSmsMessage) message = NULL;
  const char *text;
  gboolean message_sent;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION (self));

  text = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));

  if (!g_utf8_strlen (text, -1))
    return;

  participants = g_hash_table_get_keys (self->participants);
  message = g_object_new (VALENT_TYPE_SMS_MESSAGE,
                          "box",       VALENT_SMS_MESSAGE_BOX_OUTBOX,
                          "date",      0,
                          "id",        -1,
                          "media",     NULL,
                          "read",      FALSE,
                          "sender",    NULL,
                          "text",      text,
                          "thread-id", self->thread_id,
                          NULL);

  g_signal_emit (G_OBJECT (self),
                 signals [SEND_MESSAGE], 0,
                 participants, message,
                 &message_sent);

  if (message_sent)
    {
      // TODO: add pending
      g_debug ("FIXME: add pending to window: %s", text);
    }
  else
    {
      g_debug ("Failed to send message: %s", text);
    }

  /* Clear the entry whether we failed or not */
  gtk_editable_set_text (GTK_EDITABLE (self->message_entry), "");
}

/**
 * valent_sms_conversation_get_thread_id:
 * @conversation: a #ValentSmsConversation
 *
 * Get the thread ID for @conversation.
 *
 * Returns: the thread ID
 */
gint64
valent_sms_conversation_get_thread_id (ValentSmsConversation *conversation)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION (conversation), 0);

  return conversation->thread_id;
}

/**
 * valent_sms_conversation_set_thread_id:
 * @conversation: a #ValentSmsConversation
 * @thread_id: a thread ID
 *
 * Set the thread ID for @conversation. It is a programmer error to set the
 * thread ID more than once.
 */
void
valent_sms_conversation_set_thread_id (ValentSmsConversation *conversation,
                                       gint64                 thread_id)
{
  g_return_if_fail (VALENT_IS_SMS_CONVERSATION (conversation));
  g_return_if_fail (conversation->thread_id != 0 || thread_id > 0);

  conversation->thread_id = thread_id;

  if (conversation->message_store)
    {
      conversation->messages = valent_sms_store_dup_thread (conversation->message_store,
                                                            conversation->thread_id);
    }
}

/**
 * valent_sms_conversation_get_title:
 * @conversation: a #ValentSmsConversation
 *
 * Get the title of the conversation, usually the contact name.
 *
 * Returns: (transfer none): the conversation title
 */
const char *
valent_sms_conversation_get_title (ValentSmsConversation *conversation)
{
  gpointer *addresses;
  g_autoptr (GList) contacts = NULL;
  unsigned int n_contacts = 0;

  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION (conversation), NULL);

  if (conversation->title == NULL)
    {
      g_clear_pointer (&conversation->subtitle, g_free);

      addresses = g_hash_table_get_keys_as_array (conversation->participants,
                                                  &n_contacts);
      contacts = g_hash_table_get_values (conversation->participants);

      if (n_contacts == 0)
        {
          conversation->title = g_strdup (_("New Conversation"));
          conversation->subtitle = NULL;
        }
      else
        {
          conversation->title = e_contact_get (contacts->data,
                                               E_CONTACT_FULL_NAME);

          if (n_contacts == 1)
            {
              conversation->subtitle = g_strdup ((const char *)addresses[0]);
            }
          else
            {
              unsigned int remainder;

              remainder = n_contacts - 1;
              conversation->subtitle = g_strdup_printf (ngettext ("%u other contact",
                                                                  "%u others",
                                                                  remainder),
                                                        remainder);
            }
        }
    }

  return conversation->title;
}

/**
 * valent_sms_conversation_get_subtitle:
 * @conversation: a #ValentSmsConversation
 *
 * Get the subtitle of the conversation. If the conversation has one recipient
 * this will be its address (eg. phone number), otherwise it will be a string
 * such as "And 2 others".
 *
 * Returns: (transfer none): the conversation subtitle
 */
const char *
valent_sms_conversation_get_subtitle (ValentSmsConversation *conversation)
{
  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION (conversation), NULL);

  if (conversation->title == NULL)
    valent_sms_conversation_get_title (conversation);

  return conversation->subtitle;
}

/**
 * valent_sms_conversation_scroll_to_date:
 * @conversation: a #ValentSmsConversation
 * @date: a UNIX epoch timestamp
 *
 * Scroll to the message closest to @date.
 */
void
valent_sms_conversation_scroll_to_date (ValentSmsConversation *conversation,
                                        gint64                 date)
{
  GtkWidget *row;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION (conversation));
  g_return_if_fail (date > 0);

  /* First look through the list box */
  for (row = gtk_widget_get_first_child (GTK_WIDGET (conversation->message_list));
       row != NULL;
       row = gtk_widget_get_next_sibling (row))
    {
      if G_UNLIKELY (GTK_LIST_BOX_ROW (row) == conversation->pending)
        continue;

      /* If this message is equal or older than the target date, we're done */
      if (valent_sms_conversation_row_get_date (VALENT_SMS_CONVERSATION_ROW (row)) <= date)
        {
          valent_sms_conversation_jump_to_row (conversation, row);
          return;
        }
    }

  /* If there are no more messages, we're done */
  if (conversation->messages == NULL)
    return;

  /* Populate the list in reverse until we find the message */
  while (conversation->messages->tail != NULL)
    {
      GtkWidget *row;
      ValentSmsMessage *message;
      gint64 type;

      message = g_queue_peek_tail (conversation->messages);
      type = valent_sms_message_get_box (message);

      /* TODO: An unsupported message type */
      if (type != VALENT_SMS_MESSAGE_BOX_INBOX &&
          type != VALENT_SMS_MESSAGE_BOX_SENT)
        {
          g_warning ("Unknown message type '%li'; discarding", type);
          g_queue_pop_tail (conversation->messages);
          continue;
        }

      /* Prepend the message */
      row = valent_sms_conversation_prepend_message (conversation, message);
      g_queue_pop_tail (conversation->messages);

      /* If this message is equal or older than the target date, we're done */
      if (valent_sms_message_get_date (message) <= date)
        {
          valent_sms_conversation_jump_to_row (conversation, row);
          return;
        }
   }
}

/**
 * valent_sms_conversation_scroll_to_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentSmsMessage
 *
 * A convenience for calling valent_sms_message_get_date() and then
 * valent_sms_conversation_scroll_to_date().
 */
void
valent_sms_conversation_scroll_to_message (ValentSmsConversation *conversation,
                                           ValentSmsMessage         *message)
{
  gint64 date;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION (conversation));
  g_return_if_fail (VALENT_IS_SMS_MESSAGE (message));

  date = valent_sms_message_get_date (message);
  valent_sms_conversation_scroll_to_date (conversation, date);
}

