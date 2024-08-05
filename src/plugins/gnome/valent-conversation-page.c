// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-conversation-page"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <tracker-sparql.h>
#include <valent.h>

#include "valent-date-label.h"
#include "valent-contact-page.h"
#include "valent-contact-row.h"
#include "valent-conversation-row.h"
#include "valent-ui-utils-private.h"

#include "valent-conversation-page.h"

#define GET_THREAD_ATTACHMENTS_RQ "/ca/andyholmes/Valent/sparql/get-thread-attachments.rq"

struct _ValentConversationPage
{
  AdwNavigationPage       parent_instance;
  ValentContactStore     *contact_store;
  ValentMessagesAdapter  *message_store;
  char                   *iri;
  GListModel             *thread;
  TrackerSparqlStatement *get_thread_attachments_stmt;
  GHashTable             *participants;
  GHashTable             *outbox;
  GListStore             *attachments;

  /* Viewport state */
  double                  offset;
  unsigned int            position_bottom;
  unsigned int            position_top;
  gboolean                should_scroll;
  unsigned int            populate_id;
  unsigned int            update_id;

  /* template */
  GtkScrolledWindow      *scrolledwindow;
  GtkAdjustment          *vadjustment;
  GtkListBox             *message_list;
  GtkWidget              *message_entry;

  AdwDialog              *details_dialog;
  AdwNavigationView      *details_view;
  GtkWidget              *participant_list;
  GtkWidget              *attachment_list;
};

static void       valent_conversation_page_announce_message (ValentConversationPage *self,
                                                             ValentMessage          *message);
static gboolean   valent_conversation_page_check_message    (ValentConversationPage *self);
static void       valent_conversation_page_send_message     (ValentConversationPage *self);

G_DEFINE_FINAL_TYPE (ValentConversationPage, valent_conversation_page, ADW_TYPE_NAVIGATION_PAGE)

typedef enum {
  PROP_CONTACT_STORE = 1,
  PROP_MESSAGES,
  PROP_IRI,
} ValentConversationPageProperty;

static GParamSpec *properties[PROP_IRI + 1] = { NULL, };


static void
phone_lookup_cb (ValentContactStore *store,
                 GAsyncResult       *result,
                 GtkWidget          *widget)
{
  g_autoptr (EContact) contact = NULL;
  g_autoptr (GError) error = NULL;
  GtkWidget *conversation;

  contact = valent_contact_store_lookup_contact_finish (store, result, &error);
  if (contact == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  conversation = gtk_widget_get_ancestor (widget, VALENT_TYPE_CONVERSATION_PAGE);
  if (conversation != NULL)
    {
      ValentConversationPage *self = VALENT_CONVERSATION_PAGE (conversation);
      ValentConversationRow *row = VALENT_CONVERSATION_ROW (widget);
      ValentMessage *message = valent_conversation_row_get_message (row);
      const char *sender = valent_message_get_sender (message);

      valent_conversation_page_add_participant (self, contact, sender);
      valent_conversation_row_set_contact (row, contact);
    }
}

static void
message_list_header_func (GtkListBoxRow *row,
                          GtkListBoxRow *before,
                          gpointer       user_data)
{
  ValentConversationRow *current_row = VALENT_CONVERSATION_ROW (row);
  ValentConversationRow *prev_row = VALENT_CONVERSATION_ROW (before);
  int64_t row_date, prev_date;
  gboolean row_incoming;

  g_assert (GTK_IS_LIST_BOX_ROW (row));
  g_assert (before == NULL || GTK_IS_LIST_BOX_ROW (before));

  /* If this is an incoming message, show the avatar
   */
  row_incoming = valent_conversation_row_is_incoming (current_row);
  valent_conversation_row_show_avatar (current_row, row_incoming);

  if (before == NULL)
    return;

  /* If it's been more than an hour between messages, show a date label.
   * Otherwise, if the current and previous rows are incoming, hide the
   * previous row's avatar.
   */
  prev_date = valent_conversation_row_get_date (prev_row);
  row_date = valent_conversation_row_get_date (current_row);
  if (row_date - prev_date > G_TIME_SPAN_HOUR / 1000)
    {
      GtkWidget *header = gtk_list_box_row_get_header (row);

      if (header == NULL)
        {
          header = g_object_new (VALENT_TYPE_DATE_LABEL,
                                 "date", row_date,
                                 "mode", VALENT_DATE_FORMAT_ADAPTIVE,
                                 NULL);
          gtk_widget_add_css_class (header, "date-marker");
          gtk_widget_add_css_class (header, "dim-label");
          gtk_list_box_row_set_header (row, header);
        }
    }
  else if (valent_conversation_row_is_incoming (prev_row))
    {
      valent_conversation_row_show_avatar (prev_row, !row_incoming);
    }
}

/*< private >
 * valent_conversation_page_insert_message:
 * @conversation: a `ValentConversationPage`
 * @message: a `ValentMessage`
 * @position: position to insert the widget
 *
 * Create a new message row for @message and insert it into the message list at
 * @position.
 *
 * Returns: (transfer none): a `GtkWidget`
 */
static GtkWidget *
valent_conversation_page_insert_message (ValentConversationPage *self,
                                         ValentMessage          *message,
                                         int                     position)
{
  ValentConversationRow *row;
  const char *sender = NULL;

  g_assert (VALENT_IS_CONVERSATION_PAGE (self));
  g_assert (VALENT_IS_MESSAGE (message));

  row = g_object_new (VALENT_TYPE_CONVERSATION_ROW,
                      "message",     message,
                      "activatable", FALSE,
                      "selectable",  FALSE,
                      NULL);

  sender = valent_message_get_sender (message);
  if (sender != NULL && *sender != '\0')
    {
      EContact *contact;

      contact = g_hash_table_lookup (self->participants, sender);
      if (contact != NULL)
        {
          valent_conversation_row_set_contact (row, contact);
        }
      else
        {
          g_autoptr (GCancellable) cancellable = NULL;

          cancellable = g_cancellable_new ();
          g_signal_connect_object (row,
                                   "destroy",
                                   G_CALLBACK (g_cancellable_cancel),
                                   cancellable,
                                   G_CONNECT_SWAPPED);
          valent_contact_store_lookup_contact (self->contact_store,
                                               sender,
                                               cancellable,
                                               (GAsyncReadyCallback)phone_lookup_cb,
                                               row);
        }
    }
  else if (g_hash_table_size (self->participants) == 1)
    {
      GHashTableIter iter;
      EContact *contact;

      g_hash_table_iter_init (&iter, self->participants);
      g_hash_table_iter_next (&iter, NULL, (void **)&contact);
      valent_conversation_row_set_contact (row, contact);
    }

  gtk_list_box_insert (self->message_list, GTK_WIDGET (row), position);

  return GTK_WIDGET (row);
}

/*
 * Scrolled Window
 */
static ValentMessage *
valent_conversation_page_pop_tail (ValentConversationPage *self)
{
  ValentMessage *ret = NULL;

  g_assert (G_IS_LIST_MODEL (self->thread));

  if (self->position_top > 0)
    {
      self->position_top -= 1;
      ret = g_list_model_get_item (self->thread, self->position_top);
    }

  return ret;
}

static void
valent_conversation_page_populate_reverse (ValentConversationPage *self,
                                           unsigned int            count)
{
  unsigned int n_items;

  if G_UNLIKELY (self->thread == NULL)
    return;

  n_items = g_list_model_get_n_items (self->thread);
  if (n_items == 0)
    return;

  /* Prime the top position for the first message, so that result is the
   * top and bottom positions equivalent to the number of messages.
   */
  if (self->position_bottom == self->position_top)
    {
      self->position_top = n_items;
      self->position_bottom = n_items - 1;
    }

  for (unsigned int i = 0; i < count; i++)
    {
      g_autoptr (ValentMessage) message = NULL;

      message = valent_conversation_page_pop_tail (self);
      if (message == NULL)
        break;

      valent_conversation_page_insert_message (self, message, 0);
    }

  gtk_list_box_invalidate_headers (self->message_list);
}

static gboolean
valent_conversation_page_populate (gpointer data)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (data);
  double page_size = gtk_adjustment_get_page_size (self->vadjustment);
  double upper = gtk_adjustment_get_upper (self->vadjustment);
  double value = gtk_adjustment_get_value (self->vadjustment);

  self->offset = (upper - page_size) - value;
  self->should_scroll = TRUE;

  valent_conversation_page_populate_reverse (self, 25);
  self->populate_id = 0;

  return G_SOURCE_REMOVE;
}

static inline void
valent_conversation_page_queue_populate (ValentConversationPage *self)
{
  if (self->populate_id == 0)
    {
      self->populate_id = g_idle_add_full (G_PRIORITY_LOW,
                                           valent_conversation_page_populate,
                                           g_object_ref (self),
                                           g_object_unref);
    }
}

static gboolean
valent_conversation_page_update (gpointer data)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (data);
  double page_size = gtk_adjustment_get_page_size (self->vadjustment);

  if (self->should_scroll)
    {
      double upper = gtk_adjustment_get_upper (self->vadjustment);
      double new_value = (upper - page_size) - self->offset;

      self->offset = 0;
      self->should_scroll = FALSE;
      gtk_adjustment_set_value (self->vadjustment, new_value);
    }

  self->update_id = 0;

  return G_SOURCE_REMOVE;
}

static inline void
valent_conversation_page_queue_update (ValentConversationPage *self)
{
  if (self->update_id == 0)
    {
      self->update_id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                                         valent_conversation_page_update,
                                         g_object_ref (self),
                                         g_object_unref);
    }
}

static void
on_scroll_upper_changed (ValentConversationPage *self)
{
  if G_UNLIKELY (!gtk_widget_get_realized (GTK_WIDGET (self)))
    return;

  valent_conversation_page_queue_update (self);
}

static void
on_scroll_value_changed (ValentConversationPage *self)
{
  double page_size = gtk_adjustment_get_page_size (self->vadjustment);
  double value = gtk_adjustment_get_value (self->vadjustment);

  if (value < (page_size * 2))
    valent_conversation_page_queue_populate (self);
}

static gboolean
valent_conversation_page_is_latest (ValentConversationPage *self)
{
  double upper, value, page_size;

  value = gtk_adjustment_get_value (self->vadjustment);
  upper = gtk_adjustment_get_upper (self->vadjustment);
  page_size = gtk_adjustment_get_page_size (self->vadjustment);

  return ABS (upper - page_size - value) <= DBL_EPSILON;
}

static void
valent_conversation_page_announce_message (ValentConversationPage *self,
                                           ValentMessage          *message)
{
  g_autofree char *summary = NULL;
  GListModel *attachments;
  unsigned int n_attachments = 0;
  EContact *contact = NULL;
  const char *contact_medium = NULL;
  const char *sender = NULL;
  const char *text = NULL;

  g_assert (VALENT_IS_CONVERSATION_PAGE (self));
  g_assert (VALENT_IS_MESSAGE (message));

  if (valent_message_get_box (message) != VALENT_MESSAGE_BOX_INBOX)
    return;

  attachments = valent_message_get_attachments (message);
  if (attachments != NULL)
    n_attachments = g_list_model_get_n_items (attachments);

  contact_medium = valent_message_get_sender (message);
  if (contact_medium != NULL && *contact_medium != '\0')
    contact = g_hash_table_lookup (self->participants, contact_medium);

  if (contact == NULL && g_hash_table_size (self->participants) == 1)
    {
      GHashTableIter iter;

      g_hash_table_iter_init (&iter, self->participants);
      g_hash_table_iter_next (&iter, (void **)&sender, (void **)&contact);
    }

  if (contact != NULL)
    sender = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
  else if (sender == NULL)
    sender = _("Unknown");

  if (n_attachments == 0)
    {
      /* TRANSLATORS: This is announced to AT devices (i.e. screen readers)
       * when a new message is received.
       */
      summary = g_strdup_printf (_("New message from %s"), sender);
    }
  else
    {
      /* TRANSLATORS: This is announced to AT devices (i.e. screen readers)
       * when a new message is received with attachments.
       */
      summary = g_strdup_printf (ngettext ("New message from %s, with %d attachment",
                                           "New message from %s, with %d attachments",
                                           n_attachments),
                                 sender, n_attachments);
    }

  gtk_accessible_announce (GTK_ACCESSIBLE (self),
                           summary,
                           GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);

  // TODO: should the summary be different if the message has no text content?
  text = valent_message_get_text (message);
  if (text != NULL && *text != '\0')
    {
      gtk_accessible_announce (GTK_ACCESSIBLE (self),
                               text,
                               GTK_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_MEDIUM);
    }
}

static gboolean
valent_conversation_page_clear_outbox (ValentConversationPage *self,
                                       ValentMessage          *message)
{
  GHashTableIter iter;
  ValentMessage *expected;
  GtkWidget *row;

  if (valent_message_get_box (message) != VALENT_MESSAGE_BOX_SENT)
    return FALSE;

  g_hash_table_iter_init (&iter, self->outbox);
  while (g_hash_table_iter_next (&iter, (void **)&row, (void **)&expected))
    {
      const char *text = valent_message_get_text (message);
      const char *expected_text = valent_message_get_text (expected);
      GListModel *attachments;
      GListModel *expected_attachments;
      unsigned int n_attachments = 0;
      unsigned int n_expected_attachments = 0;

      /* TODO: Normalizing NULL and the empty string might not be the right
       *       thing to do.
       */
      text = text != NULL ? text : "";
      expected_text = expected_text != NULL ? expected_text : "";
      if (!g_str_equal (text, expected_text))
        continue;

      /* TODO: This check should compare the attachments, but it's not terribly
       *       likely there will be a conflict here.
       */
      attachments = valent_message_get_attachments (message);
      if (attachments != NULL)
        n_attachments = g_list_model_get_n_items (attachments);

      expected_attachments = valent_message_get_attachments (expected);
      if (expected_attachments != NULL)
        n_expected_attachments = g_list_model_get_n_items (expected_attachments);

      if (n_attachments != n_expected_attachments)
        continue;

      g_hash_table_iter_remove (&iter);
      gtk_list_box_remove (self->message_list, row);

      return TRUE;
    }

  return FALSE;
}

static void
on_thread_items_changed (GListModel            *model,
                         unsigned int           position,
                         unsigned int           removed,
                         unsigned int           added,
                         ValentConversationPage *self)
{
  unsigned int position_bottom, position_top;
  unsigned int position_real;

  g_assert (G_IS_LIST_MODEL (model));
  g_assert (VALENT_IS_CONVERSATION_PAGE (self));

  /* If the top and bottom positions are equal and we're being notified of
   * additions, then this must be the initial load
   */
  if (self->position_top == self->position_bottom && added > 0)
    {
      valent_conversation_page_queue_populate (self);
      return;
    }

  /* Update the internal pointers that track the thread position at the top
   * and bottom of the viewport canvas (i.e. loaded).
   */
  position_bottom = self->position_bottom;
  position_top = self->position_top;
  position_real = position - position_top;

  if (position <= position_top)
    self->position_top = position;

  if (position >= position_bottom)
    {
      self->position_bottom = position;
      self->should_scroll = valent_conversation_page_is_latest (self);
    }

  /* Load the message if the position is greater than or equal to the top
   * position, or if it's also higher than the bottom position (new message).
   */
  if (position >= position_top)
    {
      for (unsigned int i = 0; i < removed; i++)
        {
          GtkListBoxRow *row;

          row = gtk_list_box_get_row_at_index (self->message_list, position_real);
          gtk_list_box_remove (self->message_list, GTK_WIDGET (row));
        }

      for (unsigned int i = 0; i < added; i++)
        {
          g_autoptr (ValentMessage) message = NULL;

          message = g_list_model_get_item (self->thread, position + i);

          /* If this is new message, check if it matches an outbox row.
           */
          if (position >= position_bottom)
            valent_conversation_page_clear_outbox (self, message);

          valent_conversation_page_insert_message (self,
                                                   message,
                                                   position_real + i);

          /* If this is new message, announce it for AT devices.
           */
          if (position >= position_bottom)
            valent_conversation_page_announce_message (self, message);
        }
    }

  gtk_list_box_invalidate_headers (self->message_list);
}

static void
valent_conversation_page_load (ValentConversationPage *self)
{
  unsigned int n_threads = 0;

  if (self->message_store == NULL)
    return;

  n_threads = g_list_model_get_n_items (G_LIST_MODEL (self->message_store));
  for (unsigned int i = 0; i < n_threads; i++)
    {
      g_autoptr (GListModel) thread = NULL;
      g_autofree char *thread_iri = NULL;

      thread = g_list_model_get_item (G_LIST_MODEL (self->message_store), i);
      g_object_get (thread, "iri", &thread_iri, NULL);

      if (g_strcmp0 (self->iri, thread_iri) == 0)
        {
          g_set_object (&self->thread, thread);
          break;
        }
    }

  if (self->thread != NULL)
    {
      g_signal_connect_object (self->thread,
                               "items-changed",
                               G_CALLBACK (on_thread_items_changed),
                               self,
                               G_CONNECT_DEFAULT);
      on_thread_items_changed (self->thread,
                               0,
                               0,
                               g_list_model_get_n_items (self->thread),
                               self);
    }
}

/*
 * Message Entry
 */
static void
on_entry_activated (GtkEntry               *entry,
                    ValentConversationPage *self)
{
  valent_conversation_page_send_message (self);
}

static void
on_entry_changed (GtkEntry               *entry,
                  ValentConversationPage *self)
{
  valent_conversation_page_check_message (self);
}

/*< private >
 * valent_conversation_page_check_message:
 * @self: a `ValentConversationPage`
 *
 * Send the current text and/or attachment provided by the user.
 */
static gboolean
valent_conversation_page_check_message (ValentConversationPage *self)
{
  const char *text;
  gboolean ready = FALSE;

  text = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));
  if (self->attachments != NULL || (text != NULL && *text != '\0'))
    ready = TRUE;

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "message.send", ready);

  return ready;
}

static void
valent_conversation_page_send_message_cb (ValentMessagesAdapter *adapter,
                                          GAsyncResult          *result,
                                          gpointer               user_data)
{
  g_autoptr (ValentConversationPage) self = VALENT_CONVERSATION_PAGE (g_steal_pointer (&user_data));
  GError *error = NULL;

  if (valent_messages_adapter_send_message_finish (adapter, result, &error))
    {
      ValentMessage *message = g_task_get_task_data (G_TASK (result));
      GtkWidget *row;

      /* Append and scroll to the outgoing message
       */
      self->should_scroll = TRUE;
      row = g_object_new (VALENT_TYPE_CONVERSATION_ROW,
                          "message", message,
                          NULL);
      gtk_list_box_insert (GTK_LIST_BOX (self->message_list), row, -1);
      g_hash_table_replace (self->outbox,
                            g_object_ref (row),
                            g_object_ref (message));

      g_clear_object (&self->attachments);
      gtk_editable_set_text (GTK_EDITABLE (self->message_entry), "");
      gtk_widget_remove_css_class (self->message_entry, "error");
      gtk_widget_set_sensitive (self->message_entry, TRUE);
    }
  else if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
    {
      gtk_widget_add_css_class (self->message_entry, "error");
      gtk_widget_set_sensitive (self->message_entry, TRUE);
    }
  else
    {
      return;
    }

  gtk_widget_set_sensitive (self->message_entry, TRUE);
}

/*< private >
 * valent_conversation_page_send_message:
 * @self: a `ValentConversationPage`
 *
 * Send the current text and/or attachment provided by the user.
 */
static void
valent_conversation_page_send_message (ValentConversationPage *self)
{
  g_autoptr (ValentMessage) message = NULL;
  g_autoptr (GStrvBuilder) builder = NULL;
  g_auto (GStrv) recipients = NULL;
  GHashTableIter iter;
  const char *recipient;
  int64_t subscription_id;
  const char *text;

  g_assert (VALENT_IS_CONVERSATION_PAGE (self));

  text = gtk_editable_get_text (GTK_EDITABLE (self->message_entry));
  if (self->attachments == NULL && (text == NULL || *text == '\0'))
    return;

  builder = g_strv_builder_new ();
  g_hash_table_iter_init (&iter, self->participants);
  while (g_hash_table_iter_next (&iter, (void **)&recipient, NULL))
    g_strv_builder_add (builder, recipient);
  recipients = g_strv_builder_end (builder);

  // FIXME: infer from last message?
  subscription_id = -1;

  message = g_object_new (VALENT_TYPE_MESSAGE,
                          "iri",             NULL,
                          "attachments",     self->attachments,
                          "box",             VALENT_MESSAGE_BOX_OUTBOX,
                          "date",            valent_timestamp_ms (),
                          "recipients",      recipients,
                          "subscription-id", subscription_id,
                          "text",            text,
                          NULL);

  valent_messages_adapter_send_message (self->message_store,
                                        message,
                                        NULL,
                                        (GAsyncReadyCallback)valent_conversation_page_send_message_cb,
                                        g_object_ref (self));
  gtk_widget_set_sensitive (self->message_entry, FALSE);
}

/*
 * Details Dialog
 */
static ValentMessageAttachment *
valent_message_attachment_from_sparql_cursor (TrackerSparqlCursor  *cursor,
                                            GError              **error)
{
  const char *iri = NULL;
  g_autoptr (GIcon) preview = NULL;
  g_autoptr (GFile) file = NULL;

  g_assert (TRACKER_IS_SPARQL_CURSOR (cursor));
  g_assert (error == NULL || *error == NULL);

  iri = tracker_sparql_cursor_get_string (cursor, 0, NULL);
  if (tracker_sparql_cursor_is_bound (cursor, 1))
    {
      const char *base64_data;

      base64_data = tracker_sparql_cursor_get_string (cursor, 1, NULL);
      if (base64_data != NULL)
        {
          g_autoptr (GBytes) bytes = NULL;
          unsigned char *data;
          size_t len;

          data = g_base64_decode (base64_data, &len);
          bytes = g_bytes_new_take (g_steal_pointer (&data), len);
          preview = g_bytes_icon_new (bytes);
        }
    }

  if (tracker_sparql_cursor_is_bound (cursor, 2))
    {
      const char *file_uri;

      file_uri = tracker_sparql_cursor_get_string (cursor, 2, NULL);
      if (file_uri != NULL)
        file = g_file_new_for_uri (file_uri);
    }

  return g_object_new (VALENT_TYPE_MESSAGE_ATTACHMENT,
                       "iri",     iri,
                       "preview", preview,
                       "file",    file,
                       NULL);
}

static void
cursor_get_thread_attachments_cb (TrackerSparqlCursor *cursor,
                                  GAsyncResult        *result,
                                  gpointer             user_data)
{
  g_autoptr (GListStore) attachments = G_LIST_STORE (g_steal_pointer (&user_data));
  g_autoptr (GError) error = NULL;

  if (tracker_sparql_cursor_next_finish (cursor, result, &error))
    {
      ValentMessageAttachment *attachment = NULL;
      GCancellable *cancellable = NULL;

      attachment = valent_message_attachment_from_sparql_cursor (cursor, &error);
      g_list_store_append (attachments, attachment);

      cancellable = g_task_get_cancellable (G_TASK (result));
      tracker_sparql_cursor_next_async (cursor,
                                        cancellable,
                                        (GAsyncReadyCallback) cursor_get_thread_attachments_cb,
                                        g_object_ref (attachments));
    }
  else
    {
      if (error != NULL)
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      tracker_sparql_cursor_close (cursor);
    }
}

static void
execute_get_thread_attachments_cb (TrackerSparqlStatement *stmt,
                                   GAsyncResult           *result,
                                   gpointer                user_data)
{
  g_autoptr (GListStore) summary = G_LIST_STORE (g_steal_pointer (&user_data));
  g_autoptr (TrackerSparqlCursor) cursor = NULL;
  GCancellable *cancellable = NULL;
  g_autoptr (GError) error = NULL;

  cursor = tracker_sparql_statement_execute_finish (stmt, result, &error);
  if (cursor == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return;
    }

  cancellable = g_task_get_cancellable (G_TASK (result));
  tracker_sparql_cursor_next_async (cursor,
                                    cancellable,
                                    (GAsyncReadyCallback) cursor_get_thread_attachments_cb,
                                    g_object_ref (summary));
}

/*< private >
 * valent_conversation_page_get_attachments:
 * @self: a `ValentConversationPage`
 *
 * Get a list of the attachment for the thread as a `GListModel`.
 *
 * Returns: (transfer full) (nullable): a `GListModel`
 */
GListModel *
valent_conversation_page_get_attachments (ValentConversationPage *self)
{
  g_autoptr (TrackerSparqlConnection) connection = NULL;
  g_autoptr (GListStore) attachments = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  GError *error = NULL;

  g_return_val_if_fail (VALENT_IS_CONVERSATION_PAGE (self), NULL);

  g_object_get (self->message_store, "connection", &connection, NULL);
  if (self->get_thread_attachments_stmt == NULL)
    {
      self->get_thread_attachments_stmt =
        tracker_sparql_connection_load_statement_from_gresource (connection,
                                                                 GET_THREAD_ATTACHMENTS_RQ,
                                                                 cancellable,
                                                                 &error);
    }

  if (self->get_thread_attachments_stmt == NULL)
    {
      g_warning ("%s(): %s", G_STRFUNC, error->message);
      return NULL;
    }

  attachments = g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT);
  tracker_sparql_statement_bind_string (self->get_thread_attachments_stmt,
                                        "iri",
                                        self->iri);
  tracker_sparql_statement_execute_async (self->get_thread_attachments_stmt,
                                          cancellable,
                                          (GAsyncReadyCallback) execute_get_thread_attachments_cb,
                                          g_object_ref (attachments));

  return G_LIST_MODEL (g_steal_pointer (&attachments));
}

static void
on_contact_selected (ValentContactPage      *page,
                     EContact               *contact,
                     const char             *target,
                     ValentConversationPage *self)
{
  valent_conversation_page_add_participant (self, contact, target);
  adw_navigation_view_pop (self->details_view);
}

static void
on_add_participant (GtkButton              *row,
                    ValentConversationPage *self)
{
  AdwNavigationPage *page;

  g_assert (VALENT_IS_CONVERSATION_PAGE (self));

  page = g_object_new (VALENT_TYPE_CONTACT_PAGE,
                       "tag",           "contacts",
                       "contact-store", self->contact_store,
                       NULL);
  g_signal_connect_object (page,
                           "selected",
                           G_CALLBACK (on_contact_selected),
                           self,
                           G_CONNECT_DEFAULT);
  adw_navigation_view_push (self->details_view, page);
}

static void
save_attachment_cb (GtkFileDialog *dialog,
                    GAsyncResult  *result,
                    gpointer       user_data)
{
  g_autoptr (GFile) source = G_FILE (g_steal_pointer (&user_data));
  g_autoptr (GFile) target = NULL;
  g_autoptr (GError) error = NULL;

  target = gtk_file_dialog_save_finish (dialog, result, &error);
  if (target == NULL)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  g_file_copy_async (source,
                     target,
                     G_FILE_COPY_NONE,
                     G_PRIORITY_DEFAULT,
                     NULL,       /* cancellable */
                     NULL, NULL, /* progress */
                     NULL, NULL  /* task */);
}

static void
on_save_attachment (GtkButton *button,
                    GFile     *file)
{
  GtkWidget *widget = GTK_WIDGET (button);
  g_autoptr (GCancellable) cancellable = NULL;
  GtkFileDialog *dialog;

  g_assert (G_IS_FILE (file));

  dialog = g_object_new (GTK_TYPE_FILE_DIALOG,
                         "title",        _("Attach Files"),
                         "accept-label", _("Open"),
                         NULL);

  cancellable = g_cancellable_new ();
  g_signal_connect_object (widget,
                           "destroy",
                           G_CALLBACK (g_cancellable_cancel),
                           cancellable,
                           G_CONNECT_SWAPPED);

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (gtk_widget_get_root (widget)),
                        cancellable,
                        (GAsyncReadyCallback) save_attachment_cb,
                        g_object_ref (file));
}


static GtkWidget *
attachment_list_create (gpointer item,
                        gpointer user_data)
{
  ValentMessageAttachment *attachment = VALENT_MESSAGE_ATTACHMENT (item);
  GtkWidget *row;
  GtkWidget *image;
  GtkWidget *button;
  GIcon *preview;
  GFile *file;
  g_autofree char *filename = NULL;

  preview = valent_message_attachment_get_preview (attachment);
  file = valent_message_attachment_get_file (attachment);
  if (file != NULL)
    filename = g_file_get_basename (file);

  row = g_object_new (ADW_TYPE_ACTION_ROW,
                      "title",       filename,
                      "title-lines", 1,
                      NULL);

  image = g_object_new (GTK_TYPE_IMAGE,
                        "gicon",        preview,
                        "pixel-size",   48,
                        "overflow",     GTK_OVERFLOW_HIDDEN,
                        "tooltip-text", filename,
                        "halign",       GTK_ALIGN_START,
                        NULL);
  adw_action_row_add_prefix (ADW_ACTION_ROW (row), image);

  if (file != NULL)
    {
      button = g_object_new (GTK_TYPE_BUTTON,
                             "icon-name",    "document-save-symbolic",
                             "tooltip-text", _("Save"),
                             "valign",       GTK_ALIGN_CENTER,
                             NULL);
      gtk_widget_add_css_class (GTK_WIDGET (button), "circular");
      gtk_widget_add_css_class (GTK_WIDGET (button), "flat");
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), button);
      g_signal_connect_object (button,
                               "clicked",
                               G_CALLBACK (on_save_attachment),
                               file,
                               G_CONNECT_DEFAULT);
    }

  return row;
}

static void
conversation_details_action (GtkWidget  *widget,
                             const char *action_name,
                             GVariant   *parameters)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (widget);
  g_autoptr (GListModel) attachments = NULL;

  g_assert (VALENT_IS_CONVERSATION_PAGE (self));

  attachments = valent_conversation_page_get_attachments (self);
  gtk_list_box_bind_model (GTK_LIST_BOX (self->attachment_list),
                           attachments,
                           attachment_list_create,
                           NULL,
                           NULL);

  adw_dialog_present (self->details_dialog, widget);
}

static void
gtk_file_dialog_open_multiple_cb (GtkFileDialog          *dialog,
                                  GAsyncResult           *result,
                                  ValentConversationPage *self)
{
  g_autoptr (GListModel) files = NULL;
  unsigned int n_files;
  g_autoptr (GError) error = NULL;

  files = gtk_file_dialog_open_multiple_finish (dialog, result, &error);
  if (files == NULL)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  if (self->attachments == NULL)
    self->attachments = g_list_store_new (VALENT_TYPE_MESSAGE_ATTACHMENT);

  n_files = g_list_model_get_n_items (files);
  for (unsigned int i = 0; i < n_files; i++)
    {
      g_autoptr (ValentMessageAttachment) attachment = NULL;
      g_autoptr (GFile) file = NULL;

      file = g_list_model_get_item (files, i);
      attachment = g_object_new (VALENT_TYPE_MESSAGE_ATTACHMENT,
                                 "file", file,
                                 NULL);
      g_list_store_append (self->attachments, attachment);
    }

  valent_conversation_page_check_message (self);
}

/**
 * ValentSharePlugin|message.attachment:
 * @parameter: %NULL
 *
 * The default share action opens the platform-specific dialog for selecting
 * files, typically a `GtkFileChooserDialog`.
 */
static void
message_attachment_action (GtkWidget  *widget,
                           const char *action_name,
                           GVariant   *parameters)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (widget);
  g_autoptr (GCancellable) cancellable = NULL;
  GtkFileDialog *dialog;

  g_assert (VALENT_IS_CONVERSATION_PAGE (self));

  dialog = g_object_new (GTK_TYPE_FILE_DIALOG,
                         "title",        _("Attach Files"),
                         "accept-label", _("Open"),
                         NULL);

  cancellable = g_cancellable_new ();
  g_signal_connect_object (widget,
                           "destroy",
                           G_CALLBACK (g_cancellable_cancel),
                           cancellable,
                           G_CONNECT_SWAPPED);

  gtk_file_dialog_open_multiple (dialog,
                                 GTK_WINDOW (gtk_widget_get_root (widget)),
                                 cancellable,
                                 (GAsyncReadyCallback) gtk_file_dialog_open_multiple_cb,
                                 self);
}

static void
message_send_action (GtkWidget  *widget,
                     const char *action_name,
                     GVariant   *parameters)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (widget);

  g_assert (VALENT_IS_CONVERSATION_PAGE (self));

  valent_conversation_page_send_message (self);
}

/*
 * ValentConversationPage
 */
static void
valent_conversation_page_set_iri (ValentConversationPage *self,
                                  const char             *iri)
{
  g_assert (VALENT_IS_CONVERSATION_PAGE (self));
  g_assert (iri == NULL || *iri != '\0');

  if (g_set_str (&self->iri, iri))
    valent_conversation_page_load (self);
}

/*
 * AdwNavigationPage
 */
static void
valent_conversation_page_shown (AdwNavigationPage *page)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (page);

  gtk_widget_action_set_enabled (GTK_WIDGET (self), "message.send", FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (self->message_entry));

  if (ADW_NAVIGATION_PAGE_CLASS (valent_conversation_page_parent_class)->shown)
    ADW_NAVIGATION_PAGE_CLASS (valent_conversation_page_parent_class)->shown (page);
}

/*
 * GObject
 */
static void
valent_conversation_page_dispose (GObject *object)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (object);

  if (self->thread != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->thread, self);
      g_clear_object (&self->thread);
    }

  gtk_widget_dispose_template (GTK_WIDGET (object),
                               VALENT_TYPE_CONVERSATION_PAGE);

  G_OBJECT_CLASS (valent_conversation_page_parent_class)->dispose (object);
}

static void
valent_conversation_page_finalize (GObject *object)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (object);

  g_clear_object (&self->message_store);
  g_clear_object (&self->contact_store);
  g_clear_object (&self->thread);
  g_clear_pointer (&self->iri, g_free);
  g_clear_pointer (&self->participants, g_hash_table_unref);
  g_clear_pointer (&self->outbox, g_hash_table_unref);
  g_clear_object (&self->attachments);

  G_OBJECT_CLASS (valent_conversation_page_parent_class)->finalize (object);
}

static void
valent_conversation_page_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (object);

  switch ((ValentConversationPageProperty)prop_id)
    {
    case PROP_CONTACT_STORE:
      g_value_set_object (value, self->contact_store);
      break;

    case PROP_MESSAGES:
      g_value_set_object (value, self->message_store);
      break;

    case PROP_IRI:
      g_value_set_string (value, self->iri);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_conversation_page_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ValentConversationPage *self = VALENT_CONVERSATION_PAGE (object);

  switch ((ValentConversationPageProperty)prop_id)
    {
    case PROP_CONTACT_STORE:
      self->contact_store = g_value_dup_object (value);
      break;

    case PROP_MESSAGES:
      self->message_store = g_value_dup_object (value);
      break;

    case PROP_IRI:
      valent_conversation_page_set_iri (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_conversation_page_class_init (ValentConversationPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  AdwNavigationPageClass *page_class = ADW_NAVIGATION_PAGE_CLASS (klass);

  object_class->dispose = valent_conversation_page_dispose;
  object_class->finalize = valent_conversation_page_finalize;
  object_class->get_property = valent_conversation_page_get_property;
  object_class->set_property = valent_conversation_page_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-conversation-page.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, message_list);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, message_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, scrolledwindow);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, vadjustment);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, details_dialog);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, details_view);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, participant_list);
  gtk_widget_class_bind_template_child (widget_class, ValentConversationPage, attachment_list);

  gtk_widget_class_bind_template_callback (widget_class, on_scroll_upper_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_scroll_value_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_add_participant);
  gtk_widget_class_install_action (widget_class, "conversation.details", NULL, conversation_details_action);
  gtk_widget_class_install_action (widget_class, "message.attachment", NULL, message_attachment_action);
  gtk_widget_class_install_action (widget_class, "message.send", NULL, message_send_action);

  page_class->shown = valent_conversation_page_shown;

  /**
   * ValentConversationPage:contact-store:
   *
   * The `ValentContactStore` providing `EContact` objects for the conversation.
   */
  properties [PROP_CONTACT_STORE] =
    g_param_spec_object ("contact-store", NULL, NULL,
                         VALENT_TYPE_CONTACT_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentConversationPage:messages:
   *
   * The `ValentMessagesAdapter` providing `ValentMessage` objects for the
   * conversation.
   */
  properties [PROP_MESSAGES] =
    g_param_spec_object ("messages", NULL, NULL,
                         VALENT_TYPE_MESSAGES_ADAPTER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentConversationPage:iri:
   *
   * The thread ID of the conversation.
   */
  properties [PROP_IRI] =
    g_param_spec_string ("iri", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static uint32_t
contact_medium_hash (gconstpointer medium)
{
  g_autoptr (EPhoneNumber) number = NULL;
  g_autofree char *number_str = NULL;

  if G_UNLIKELY (g_strrstr (medium, "@"))
    return g_str_hash (medium);

  number = e_phone_number_from_string (medium, NULL, NULL);
  number_str = e_phone_number_to_string (number, E_PHONE_NUMBER_FORMAT_E164);

  return g_str_hash (number_str);
}

static gboolean
contact_medium_equal (gconstpointer medium1,
                      gconstpointer medium2)
{
  if G_UNLIKELY (g_strrstr (medium1, "@") || g_strrstr (medium1, "@"))
    return g_str_equal (medium1, medium2);

  return e_phone_number_compare_strings (medium1, medium2, NULL) != E_PHONE_NUMBER_MATCH_NONE;
}

static void
valent_conversation_page_init (ValentConversationPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->message_list,
                                message_list_header_func,
                                self, NULL);

  self->participants = g_hash_table_new_full (contact_medium_hash,
                                              contact_medium_equal,
                                              g_free,
                                              g_object_unref);
  self->outbox = g_hash_table_new_full (NULL,
                                        NULL,
                                        g_object_unref,
                                        g_object_unref);
}

GtkWidget *
valent_conversation_page_new (ValentContactStore *contacts,
                              ValentMessagesAdapter *messages)
{
  return g_object_new (VALENT_TYPE_CONVERSATION_PAGE,
                       "contact-store", contacts,
                       "messages", messages,
                       NULL);
}

/**
 * valent_conversation_page_get_iri:
 * @conversation: a `ValentConversationPage`
 *
 * Get the thread IRI for @conversation.
 *
 * Returns: the thread IRI
 */
const char *
valent_conversation_page_get_iri (ValentConversationPage *conversation)
{
  g_return_val_if_fail (VALENT_IS_CONVERSATION_PAGE (conversation), NULL);

  return conversation->iri;
}

/**
 * valent_conversation_page_add_participant:
 * @conversation: a `ValentConversationPage`
 * @contact: an `EContact`
 * @medium: a contact IRI
 *
 * Add @contact to @conversation, with the contact point @medium.
 */
void
valent_conversation_page_add_participant (ValentConversationPage *conversation,
                                          EContact               *contact,
                                          const char             *medium)
{
  GHashTableIter iter;
  GtkWidget *child;
  size_t position = 0;
  gboolean is_new = FALSE;

  g_return_if_fail (VALENT_IS_CONVERSATION_PAGE (conversation));
  g_return_if_fail (E_IS_CONTACT (contact));
  g_return_if_fail (medium != NULL && *medium != '\0');

  // FIXME: use vmo:hasParticipant
  is_new = g_hash_table_replace (conversation->participants,
                                 g_strdup (medium),
                                 g_object_ref (contact));
  if (!is_new)
    return;

  /* Clear the dialog
   */
  child = gtk_widget_get_first_child (conversation->participant_list);
  while (child != NULL)
    {
      gtk_list_box_remove (GTK_LIST_BOX (conversation->participant_list), child);
      child = gtk_widget_get_first_child (conversation->participant_list);
    }

  /* Update the dialog
   */
  g_hash_table_iter_init (&iter, conversation->participants);
  while (g_hash_table_iter_next (&iter, (void **)&medium, (void **)&contact))
    {
      const char *name = NULL;

      name = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
      adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (conversation), name);

      child = g_object_new (VALENT_TYPE_CONTACT_ROW,
                            "contact",        contact,
                            "contact-medium", medium,
                            NULL);
      gtk_list_box_insert (GTK_LIST_BOX (conversation->participant_list),
                           child,
                           position++);
    }
}

static void
valent_conversation_page_scroll_to_row (ValentConversationPage *self,
                                        GtkWidget              *row)
{
  GtkWidget *viewport;
  double upper, page_size;
  double target, maximum;

  upper = gtk_adjustment_get_upper (self->vadjustment);
  page_size = gtk_adjustment_get_page_size (self->vadjustment);
  maximum = upper - page_size;
  target = upper - page_size;

  if (row != NULL)
    {
      graphene_rect_t row_bounds;
      graphene_point_t row_point;
      graphene_point_t target_point;

      viewport = gtk_scrolled_window_get_child (self->scrolledwindow);
      if (!gtk_widget_compute_bounds (row, viewport, &row_bounds))
        {
          g_warning ("%s(): failed to scroll to row", G_STRFUNC);
          return;
        }

      graphene_rect_get_bottom_right (&row_bounds, &row_point);
      if (!gtk_widget_compute_point (row, viewport, &row_point, &target_point))
        {
          g_warning ("%s(): failed to scroll to row", G_STRFUNC);
          return;
        }

      target = target_point.y;
    }

  gtk_scrolled_window_set_kinetic_scrolling (self->scrolledwindow, FALSE);
  gtk_adjustment_set_value (self->vadjustment, CLAMP (target, 0, maximum));
  gtk_scrolled_window_set_kinetic_scrolling (self->scrolledwindow, TRUE);
}

/**
 * valent_conversation_page_scroll_to_date:
 * @page: a `ValentConversationPage`
 * @date: a UNIX epoch timestamp
 *
 * Scroll to the message closest to @date.
 */
void
valent_conversation_page_scroll_to_date (ValentConversationPage *page,
                                         int64_t                 date)
{
  GtkWidget *row;
  ValentMessage *message;

  g_return_if_fail (VALENT_IS_CONVERSATION_PAGE (page));
  g_return_if_fail (date > 0);

  /* First look through the list box */
  for (row = gtk_widget_get_last_child (GTK_WIDGET (page->message_list));
       row != NULL;
       row = gtk_widget_get_prev_sibling (row))
    {
      /* If this message is equal or older than the target date, we're done
       */
      if (valent_conversation_row_get_date (VALENT_CONVERSATION_ROW (row)) <= date)
        {
          valent_conversation_page_scroll_to_row (page, row);
          return;
        }
    }

  /* If there are no more messages, we're done
   */
  g_return_if_fail (G_IS_LIST_MODEL (page->thread));

  /* Populate the list in reverse until we find the message
   */
  while ((message = valent_conversation_page_pop_tail (page)) != NULL)
    {
      /* Prepend the message
       */
      row = valent_conversation_page_insert_message (page, message, 0);
      g_object_unref (message);

      /* If this message is equal or older than the target date, we're done
       */
      if (valent_message_get_date (message) <= date)
        {
          valent_conversation_page_scroll_to_row (page, row);
          return;
        }
   }
}

/**
 * valent_conversation_page_scroll_to_message:
 * @page: a `ValentConversationPage`
 * @message: a `ValentMessage`
 *
 * A convenience for calling valent_message_get_date() and then
 * valent_conversation_page_scroll_to_date().
 */
void
valent_conversation_page_scroll_to_message (ValentConversationPage *page,
                                            ValentMessage          *message)
{
  int64_t date;

  g_return_if_fail (VALENT_IS_CONVERSATION_PAGE (page));
  g_return_if_fail (VALENT_IS_MESSAGE (message));

  date = valent_message_get_date (message);
  valent_conversation_page_scroll_to_date (page, date);
}

