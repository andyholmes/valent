// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-conversation"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libvalent-contacts.h>

#include "valent-date-label.h"
#include "valent-message.h"
#include "valent-message-thread.h"
#include "valent-sms-conversation.h"
#include "valent-sms-conversation-row.h"
#include "valent-sms-store.h"


struct _ValentSmsConversation
{
  GtkWidget           parent_instance;

  /* Template Widgets */
  GtkWidget          *message_view;
  GtkListBox         *message_list;
  GtkWidget          *message_entry;
  GtkListBoxRow      *pending;

  /* Population */
  guint               populate_id;
  guint               update_id;
  double              offset;
  GtkAdjustment      *vadjustment;

  /* Thread Resources */
  gint64              loaded_id;
  gint64              thread_id;
  ValentSmsStore     *message_store;
  GListModel         *thread;
  unsigned int        position_upper;
  unsigned int        position_lower;
  ValentContactStore *contact_store;
  GHashTable         *participants;

  char               *title;
  char               *subtitle;
};

static void   valent_sms_conversation_send_message (ValentSmsConversation *self);

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


/* Callbacks */
static void
phone_lookup_cb (ValentContactStore *store,
                 GAsyncResult       *result,
                 GtkWidget          *widget)
{
  g_autoptr (ValentSmsConversationRow) row = VALENT_SMS_CONVERSATION_ROW (widget);
  g_autoptr (EContact) contact = NULL;
  g_autoptr (GError) error = NULL;
  GtkWidget *conversation;

  contact = valent_contact_store_dup_for_phone_finish (store, result, &error);

  if (contact == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  conversation = gtk_widget_get_ancestor (widget, VALENT_TYPE_SMS_CONVERSATION);

  if (conversation != NULL)
    {
      ValentSmsConversation *self = VALENT_SMS_CONVERSATION (conversation);
      ValentMessage *message;
      const char *sender;

      message = valent_sms_conversation_row_get_message (row);
      sender = valent_message_get_sender (message);

      g_hash_table_insert (self->participants,
                           g_strdup (sender),
                           g_object_ref (contact));

      valent_sms_conversation_row_set_contact (row, contact);
    }
}

static void
valent_sms_conversation_scroll_to_row (ValentSmsConversation *self,
                                       GtkWidget             *widget)
{
  GtkScrolledWindow *scrolled = GTK_SCROLLED_WINDOW (self->message_view);
  GtkWidget *viewport;
  double upper, page_size;
  double x, y;

  /* Get the scrolled window state */
  upper = gtk_adjustment_get_upper (self->vadjustment);
  page_size = gtk_adjustment_get_page_size (self->vadjustment);

  /* Get the widget's position in the window */
  viewport = gtk_scrolled_window_get_child (scrolled);
  gtk_widget_translate_coordinates (widget, viewport, 0, 0, &x, &y);

  /* Scroll to the position */
  gtk_adjustment_set_value (self->vadjustment, CLAMP (y, page_size, upper));
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
 * valent_sms_conversation_insert_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentMessage
 * @position: position to insert the widget
 *
 * Create a new message row for @message and insert it into the message list at
 * @position.
 *
 * Returns: (transfer none): a #GtkWidget
 */
static GtkWidget *
valent_sms_conversation_insert_message (ValentSmsConversation *self,
                                        ValentMessage         *message,
                                        int                    position)
{
  ValentSmsConversationRow *row;
  const char *sender = NULL;
  EContact *contact = NULL;

  g_assert (VALENT_IS_SMS_CONVERSATION (self));
  g_assert (VALENT_IS_MESSAGE (message));

  /* Create the row */
  row = g_object_new (VALENT_TYPE_SMS_CONVERSATION_ROW,
                      "message",     message,
                      "activatable", FALSE,
                      "selectable",  FALSE,
                      NULL);

  /* If the message has a sender, try to lookup the contact */
  if ((sender = valent_message_get_sender (message)) != NULL)
    {
      GHashTableIter iter;
      const char *address = NULL;

      g_hash_table_iter_init (&iter, self->participants);

      while (g_hash_table_iter_next (&iter, (void **)&address, (void **)&contact))
        {
          if (valent_phone_number_equal (sender, address))
            {
              valent_sms_conversation_row_set_contact (row, contact);
              break;
            }

          contact = NULL;
        }

      if (contact == NULL)
        {
          valent_contact_store_dup_for_phone_async (self->contact_store,
                                                    sender,
                                                    NULL,
                                                    (GAsyncReadyCallback)phone_lookup_cb,
                                                    g_object_ref_sink (row));
        }
    }

  /* Insert the row into the message list */
  gtk_list_box_insert (self->message_list, GTK_WIDGET (row), position);

  return GTK_WIDGET (row);
}

/**
 * valent_conversation_remove_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentMessage
 *
 * Remove a message from the conversation.
 */
static void
valent_sms_conversation_remove_message (ValentSmsConversation *conversation,
                                        gint64                 message_id)
{
  GtkWidget *child;

  g_assert (VALENT_IS_SMS_CONVERSATION (conversation));
  g_assert (message_id > 0);

  for (child = gtk_widget_get_first_child (GTK_WIDGET (conversation->message_list));
       child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      ValentSmsConversationRow *row = VALENT_SMS_CONVERSATION_ROW (child);

      if (valent_sms_conversation_row_get_id (row) == message_id)
        {
          gtk_list_box_remove (conversation->message_list, child);
          break;
        }
    }
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
static inline ValentMessage *
valent_sms_conversation_pop_tail (ValentSmsConversation *self)
{
  if G_UNLIKELY (self->thread == NULL)
    return NULL;

  if (self->position_lower == 0)
    return NULL;

  self->position_lower -= 1;

  return g_list_model_get_item (self->thread, self->position_lower);
}

static inline ValentMessage *
valent_sms_conversation_pop_head (ValentSmsConversation *self)
{
  if G_UNLIKELY (self->thread == NULL)
    return NULL;

  if (self->position_upper == g_list_model_get_n_items (self->thread) - 1)
    return NULL;

  self->position_upper += 1;

  return g_list_model_get_item (self->thread, self->position_upper);
}

static void
valent_sms_conversation_populate_reverse (ValentSmsConversation *self)
{
  unsigned int count = 10;
  unsigned int n_items;

  if G_UNLIKELY (self->thread == NULL)
    return;

  if ((n_items = g_list_model_get_n_items (self->thread)) == 0)
    return;

  if (self->position_upper == self->position_lower)
    {
      self->position_lower = n_items;
      self->position_upper = n_items - 1;
    }

  for (unsigned int i = 0; i < count; i++)
    {
      g_autoptr (ValentMessage) message = NULL;

      if ((message = valent_sms_conversation_pop_tail (self)) == NULL)
        break;

      valent_sms_conversation_insert_message (self, message, 0);
    }

  gtk_list_box_invalidate_headers (self->message_list);
}

static gboolean
valent_sms_conversation_populate (gpointer data)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (data);
  double upper, value;

  upper = gtk_adjustment_get_upper (self->vadjustment);
  value = gtk_adjustment_get_value (self->vadjustment);

  self->offset = upper - value;
  valent_sms_conversation_populate_reverse (self);
  self->populate_id = 0;

  return G_SOURCE_REMOVE;
}

static inline void
valent_sms_conversation_queue_populate (ValentSmsConversation *self)
{
  if (self->populate_id > 0)
    return;

  self->populate_id = g_idle_add_full (G_PRIORITY_LOW,
                                       valent_sms_conversation_populate,
                                       g_object_ref (self),
                                       g_object_unref);
}

static gboolean
valent_sms_conversation_update (gpointer data)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (data);
  double value;

  if (self->offset > 0)
    {
      value = gtk_adjustment_get_upper (self->vadjustment) - self->offset;
      self->offset = 0;
      gtk_adjustment_set_value (self->vadjustment, value);
    }

  self->update_id = 0;

  return G_SOURCE_REMOVE;
}

static inline void
valent_sms_conversation_queue_update (ValentSmsConversation *self)
{
  if (self->update_id > 0)
    return;

  self->update_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                     valent_sms_conversation_update,
                                     g_object_ref (self),
                                     g_object_unref);
}

static void
on_edge_overshot (GtkScrolledWindow     *scrolled_window,
                  GtkPositionType        pos,
                  ValentSmsConversation *self)
{
  if (pos == GTK_POS_TOP)
    valent_sms_conversation_queue_populate (self);

  if (pos == GTK_POS_BOTTOM)
    self->offset = 0;
}

static void
on_scroll_notify_upper (GtkAdjustment         *adjustment,
                        GParamSpec            *pspec,
                        ValentSmsConversation *self)
{
  if G_UNLIKELY (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  valent_sms_conversation_queue_update (self);
}

static void
on_scroll_value_changed (GtkAdjustment         *adjustment,
                         ValentSmsConversation *self)
{
  double page_size;

  if G_UNLIKELY (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  if ((page_size = gtk_adjustment_get_page_size (adjustment)) == 0)
    return;

  if (gtk_adjustment_get_value (adjustment) < page_size * 2)
    valent_sms_conversation_populate (self);
}

static void
on_thread_items_changed (GListModel            *model,
                         unsigned int           position,
                         unsigned int           removed,
                         unsigned int           added,
                         ValentSmsConversation *self)
{
  unsigned int position_upper, position_lower;
  unsigned int position_real;
  int diff;

  g_assert (VALENT_IS_MESSAGE_THREAD (model));
  g_assert (VALENT_IS_SMS_CONVERSATION (self));

  position_upper = self->position_upper;
  position_lower = self->position_lower;
  position_real = position_lower + position;

  /* First update the internal pointers */
  diff = added - removed;

  if (position <= position_lower)
    self->position_lower += diff;

  if (position <= position_upper)
    self->position_upper += diff;

  /* If the upper and lower are equal and we're being notified of additions,
   * then this must be the initial load */
  if (self->position_lower == self->position_upper && added)
    {
      valent_sms_conversation_queue_populate (self);
      return;
    }

  /* If the position is in between our pointers we have to handle them */
  if (position >= position_lower && position <= position_upper)
    {
      /* Removals first */
      for (unsigned int i = 0; i < removed; i++)
        {
          GtkListBoxRow *row;

          row = gtk_list_box_get_row_at_index (self->message_list, position_real);
          gtk_list_box_remove (self->message_list, GTK_WIDGET (row));
        }

      /* Additions */
      for (unsigned int i = 0; i < added; i++)
        {
          g_autoptr (ValentMessage) message = NULL;

          message = g_list_model_get_item (self->thread, position + i);
          valent_sms_conversation_insert_message (self, message, position_real + i);
        }
    }
}

static void
valent_sms_conversation_load (ValentSmsConversation *self)
{
  if (self->message_store == NULL || self->thread_id == self->loaded_id)
    return;

  if (!gtk_widget_get_mapped (GTK_WIDGET (self)))
    return;

  self->loaded_id = self->thread_id;
  self->thread = valent_sms_store_get_thread (self->message_store,
                                              self->thread_id);
  g_signal_connect (self->thread,
                    "items-changed",
                    G_CALLBACK (on_thread_items_changed),
                    self);
}

static void
valent_sms_conversation_send_message (ValentSmsConversation *self)
{
  g_autoptr (ValentMessage) message = NULL;
  GVariantBuilder builder, addresses;
  GHashTableIter iter;
  gpointer address;
  int sub_id = -1;
  const char *text;
  gboolean sent;

  g_assert (VALENT_IS_SMS_CONVERSATION (self));

  text = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));

  if (!g_utf8_strlen (text, -1))
    return;

  // Metadata
  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  // Addresses
  g_variant_builder_init (&addresses, G_VARIANT_TYPE_ARRAY);
  g_hash_table_iter_init (&iter, self->participants);

  while (g_hash_table_iter_next (&iter, &address, NULL))
    g_variant_builder_add_parsed (&addresses, "{'address': <%s>}", address);

  g_variant_builder_add (&builder, "{sv}", "addresses",
                         g_variant_builder_end (&addresses));


  // TODO: SIM Card
  g_variant_builder_add (&builder, "{sv}", "sub_id",
                         g_variant_new_int64 (sub_id));

  message = g_object_new (VALENT_TYPE_MESSAGE,
                          "box",       VALENT_MESSAGE_BOX_OUTBOX,
                          "date",      0,
                          "id",        -1,
                          "metadata",  g_variant_builder_end (&builder),
                          "read",      FALSE,
                          "sender",    NULL,
                          "text",      text,
                          "thread-id", self->thread_id,
                          NULL);

  g_signal_emit (G_OBJECT (self), signals [SEND_MESSAGE], 0, message, &sent);

  if (sent)
    VALENT_TODO ("Add pending message to conversation");
  else
    g_warning ("%s(): failed sending message \"%s\"", G_STRFUNC, text);

  /* Clear the entry whether we failed or not */
  gtk_editable_set_text (GTK_EDITABLE (self->message_entry), "");
}


/*
 * GtkWidget
 */
static void
valent_sms_conversation_map (GtkWidget *widget)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (widget);

  GTK_WIDGET_CLASS (valent_sms_conversation_parent_class)->map (widget);

  gtk_widget_grab_focus (self->message_entry);
  valent_sms_conversation_load (self);
}


/*
 * GObject
 */
static void
valent_sms_conversation_dispose (GObject *object)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (object);

  if (self->thread != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->thread, self);
      g_clear_object (&self->thread);
    }

  g_clear_pointer (&self->message_view, gtk_widget_unparent);
  g_clear_pointer (&self->message_entry, gtk_widget_unparent);

  G_OBJECT_CLASS (valent_sms_conversation_parent_class)->dispose (object);
}

static void
valent_sms_conversation_finalize (GObject *object)
{
  ValentSmsConversation *self = VALENT_SMS_CONVERSATION (object);

  g_clear_object (&self->message_store);
  g_clear_object (&self->contact_store);
  g_clear_pointer (&self->participants, g_hash_table_unref);
  g_clear_pointer (&self->title, g_free);
  g_clear_pointer (&self->subtitle, g_free);

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
      valent_sms_conversation_set_thread_id (self, g_value_get_int64 (value));
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

  object_class->dispose = valent_sms_conversation_dispose;
  object_class->finalize = valent_sms_conversation_finalize;
  object_class->get_property = valent_sms_conversation_get_property;
  object_class->set_property = valent_sms_conversation_set_property;

  widget_class->map = valent_sms_conversation_map;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sms/valent-sms-conversation.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, message_list);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, message_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, message_view);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsConversation, pending);

  gtk_widget_class_bind_template_callback (widget_class, on_edge_overshot);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_icon_release);

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
   * The #ValentSmsStore providing #ValentMessage objects for the
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
   * @message: a message
   *
   * The #ValentSmsConversation::send-message signal is emitted when a user is
   * sending an outgoing message.
   *
   * The signal handler should return a boolean indicating success, although
   * this only indicates the request was sent to the device.
   */
  signals [SEND_MESSAGE] =
    g_signal_new ("send-message",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  g_signal_accumulator_first_wins, NULL, NULL,
                  G_TYPE_BOOLEAN, 1, VALENT_TYPE_MESSAGE);
}

static void
valent_sms_conversation_init (ValentSmsConversation *self)
{
  GtkScrolledWindow *scrolled;

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Watch the scroll position */
  scrolled = GTK_SCROLLED_WINDOW (self->message_view);
  self->vadjustment = gtk_scrolled_window_get_vadjustment (scrolled);
  g_signal_connect_after (self->vadjustment,
                          "notify::upper",
                          G_CALLBACK (on_scroll_notify_upper),
                          self);

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
 * Set the thread ID for @conversation.
 */
void
valent_sms_conversation_set_thread_id (ValentSmsConversation *conversation,
                                       gint64                 thread_id)
{
  GtkWidget *parent = GTK_WIDGET (conversation->message_list);
  GtkWidget *child;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION (conversation));
  g_return_if_fail (thread_id >= 0);

  if (conversation->thread_id == thread_id)
    return;

  /* Clear the current messages */
  if (conversation->thread != NULL)
    {
      g_signal_handlers_disconnect_by_data (conversation->thread, conversation);
      g_clear_object (&conversation->thread);
    }

  while ((child = gtk_widget_get_first_child (parent)))
    gtk_list_box_remove (conversation->message_list, child);

  /* Notify before beginning the load task */
  conversation->thread_id = thread_id;
  g_object_notify_by_pspec (G_OBJECT (conversation), properties [PROP_THREAD_ID]);

  /* Load the new thread */
  valent_sms_conversation_load (conversation);
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
  g_autofree char **addresses = NULL;
  g_autoptr (GList) contacts = NULL;
  unsigned int n_contacts = 0;

  g_return_val_if_fail (VALENT_IS_SMS_CONVERSATION (conversation), NULL);

  if (conversation->title == NULL)
    {
      g_clear_pointer (&conversation->subtitle, g_free);

      addresses = (char **)g_hash_table_get_keys_as_array (conversation->participants,
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
  ValentMessage *message;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION (conversation));
  g_return_if_fail (date > 0);

  /* First look through the list box */
  for (row = gtk_widget_get_last_child (GTK_WIDGET (conversation->message_list));
       row != NULL;
       row = gtk_widget_get_prev_sibling (row))
    {
      if G_UNLIKELY (GTK_LIST_BOX_ROW (row) == conversation->pending)
        continue;

      /* If this message is equal or older than the target date, we're done */
      if (valent_sms_conversation_row_get_date (VALENT_SMS_CONVERSATION_ROW (row)) <= date)
        {
          valent_sms_conversation_scroll_to_row (conversation, row);
          return;
        }
    }

  /* If there are no more messages, we're done */
  g_return_if_fail (VALENT_IS_MESSAGE_THREAD (conversation->thread));

  /* Populate the list in reverse until we find the message */
  while ((message = valent_sms_conversation_pop_tail (conversation)) != NULL)
    {
      /* Prepend the message */
      row = valent_sms_conversation_insert_message (conversation, message, 0);
      g_object_unref (message);

      /* If this message is equal or older than the target date, we're done */
      if (valent_message_get_date (message) <= date)
        {
          valent_sms_conversation_scroll_to_row (conversation, row);
          return;
        }
   }
}

/**
 * valent_sms_conversation_scroll_to_message:
 * @conversation: a #ValentSmsConversation
 * @message: a #ValentMessage
 *
 * A convenience for calling valent_message_get_date() and then
 * valent_sms_conversation_scroll_to_date().
 */
void
valent_sms_conversation_scroll_to_message (ValentSmsConversation *conversation,
                                           ValentMessage         *message)
{
  gint64 date;

  g_return_if_fail (VALENT_IS_SMS_CONVERSATION (conversation));
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  date = valent_message_get_date (message);
  valent_sms_conversation_scroll_to_date (conversation, date);
}

