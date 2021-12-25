// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-window"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <libvalent-contacts.h>

#include "valent-contact-row.h"
#include "valent-sms-conversation.h"
#include "valent-sms-message.h"
#include "valent-sms-store.h"
#include "valent-sms-window.h"
#include "valent-message-row.h"


struct _ValentSmsWindow
{
  AdwApplicationWindow  parent_instance;

  ValentContactStore   *contact_store;
  ValentSmsStore       *message_store;

  /* Template Widgets */
  AdwLeaflet           *content_box;
  AdwHeaderBar         *content_header;
  AdwHeaderBar         *sidebar_header;

  GtkLabel             *content_title;
  GtkBox               *content_layout;
  GtkListBox           *conversation_list;
  GtkStack             *content;

  GtkWidget            *message_search;
  GtkEntry             *message_search_entry;
  GtkListBox           *message_search_list;

  GtkWidget            *contact_search;
  GtkListBox           *contact_search_list;
  GtkSearchEntry       *contact_search_entry;
  GtkWidget            *dynamic_contact;
};

G_DEFINE_TYPE (ValentSmsWindow, valent_sms_window, ADW_TYPE_APPLICATION_WINDOW)

enum {
  PROP_0,
  PROP_CONTACT_STORE,
  PROP_MESSAGE_STORE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  SEND_MESSAGE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


/*
 * Generic callback for querying a contact for a widget
 */
static void
phone_lookup_cb (ValentContactStore *model,
                 GAsyncResult       *result,
                 GtkWidget          *widget)
{
  g_autoptr (GError) error = NULL;
  EContact *contact;

  contact = valent_contact_store_dup_for_phone_finish (model, result, &error);

  if (contact == NULL)
    g_warning ("Failed to get contact: %s", error->message);
  else
    g_object_set (widget, "contact", contact, NULL);

  g_clear_object (&contact);
}

static void
query_contacts_cb (ValentContactStore *model,
                   GAsyncResult       *result,
                   ValentSmsWindow    *window)
{
  g_autoptr (GError) error = NULL;
  g_autoslist (GObject) contacts = NULL;

  contacts = valent_contact_store_query_finish (model, result, &error);

  for (const GSList *iter = contacts; iter; iter = iter->next)
    valent_list_add_contact (window->contact_search_list, iter->data);
}

static void
find_messages_cb (ValentSmsStore  *store,
                  GAsyncResult    *result,
                  ValentSmsWindow *window)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GPtrArray) messages = NULL;

  messages = valent_sms_store_find_finish (store, result, &error);

  for (unsigned int i = 0; i < messages->len; i++)
    {
      g_autoptr (ValentSmsMessage) message = NULL;
      GtkWidget *row;
      const char *address;

      message = g_ptr_array_index (messages, i);
      address = valent_sms_message_get_sender (message);

      if (address == NULL)
        {
          g_warning ("Message has no sender");
          continue;
        }

      row = g_object_new (VALENT_TYPE_MESSAGE_ROW,
                          "message", message,
                          NULL);
      gtk_list_box_insert (window->message_search_list, row, -1);

      valent_contact_store_dup_for_phone_async (window->contact_store,
                                                address,
                                                NULL,
                                                (GAsyncReadyCallback)phone_lookup_cb,
                                                row);
    }
}

/*
 * Reset search pages in a timeout source
 */
static gboolean
reset_search_cb (gpointer data)
{
  ValentSmsWindow *self = data;

  g_assert (VALENT_IS_SMS_WINDOW (self));

  gtk_editable_set_text (GTK_EDITABLE (self->contact_search_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->message_search_entry), "");

  return G_SOURCE_REMOVE;
}

/*
 * Message Search
 */
static void
search_header_func (GtkListBoxRow *row,
                    GtkListBoxRow *before,
                    gpointer       user_data)
{
  if (VALENT_IS_MESSAGE_ROW (row))
    {
      if (before == NULL || !VALENT_IS_MESSAGE_ROW (before))
        {
          GtkWidget *label;
          GtkStyleContext *style;

          label = g_object_new (GTK_TYPE_LABEL,
                                "label",        _("Conversations"),
                                "halign",       GTK_ALIGN_START,
                                "margin-end",   6,
                                "margin-start", 6,
                                "margin-top",   6,
                                NULL);
          style = gtk_widget_get_style_context (label);
          gtk_style_context_add_class (style, "dim-label");
          gtk_style_context_add_class (style, "list-header-title");
          gtk_list_box_row_set_header (row, label);
        }
    }

  valent_contact_row_header_func (row, before, user_data);
}

static void
on_message_search_changed (GtkSearchEntry  *entry,
                           ValentSmsWindow *window)
{
  GtkWidget *child;
  const char *query_str;
  EBookQuery *queries[2];
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  query_str = gtk_editable_get_text (GTK_EDITABLE (entry));

  /* Clear previous results */
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (window->message_search_list))))
    gtk_list_box_remove (window->message_search_list, child);

  /* NULL query */
  if (g_strcmp0 (query_str, "") == 0)
    return;

  /* Search messages */
  valent_sms_store_find_async (window->message_store,
                               query_str,
                               NULL,
                               (GAsyncReadyCallback)find_messages_cb,
                               window);

  /* Search contacts */
  queries[0] = e_book_query_field_test (E_CONTACT_FULL_NAME,
                                        E_BOOK_QUERY_CONTAINS,
                                        query_str);
  queries[1] = e_book_query_field_test (E_CONTACT_TEL,
                                        E_BOOK_QUERY_CONTAINS,
                                        query_str);

  query = e_book_query_or (G_N_ELEMENTS (queries), queries, TRUE);
  sexp = e_book_query_to_string (query);

  valent_contact_store_query (window->contact_store,
                              sexp,
                              NULL,
                              (GAsyncReadyCallback)query_contacts_cb,
                              window);
}

static void
on_message_selected (GtkListBox      *box,
                     GtkListBoxRow   *row,
                     ValentSmsWindow *self)
{
  EContact *contact;

  if (VALENT_IS_MESSAGE_ROW (row))
    {
      ValentSmsConversation *conversation;
      ValentSmsMessage *message;
      gint64 thread_id;

      message = valent_message_row_get_message (VALENT_MESSAGE_ROW (row));
      contact = valent_message_row_get_contact (VALENT_MESSAGE_ROW (row));
      thread_id = valent_sms_message_get_thread_id (message);

      valent_sms_window_set_active_thread (self, thread_id);
      conversation = VALENT_SMS_CONVERSATION (gtk_stack_get_visible_child (self->content));
      valent_sms_conversation_scroll_to_message (conversation, message);

      /* Reset the search after the transition */
      g_timeout_add_seconds (1, reset_search_cb, self);
    }
  else if (VALENT_IS_CONTACT_ROW (row))
    {
      contact = valent_contact_row_get_contact (VALENT_CONTACT_ROW (row));
      g_debug ("CONTACT ROW SELECTED %s",
               (const char *)e_contact_get_const (contact, E_CONTACT_FULL_NAME));
    }
}

/*
 * Contact Search
 */
static gboolean
check_number (const char *query)
{
  static GRegex *is_number = NULL;

  if (is_number == NULL)
    is_number = g_regex_new ("(?!0)[\\d]{3,}", G_REGEX_OPTIMIZE, 0, NULL);

  return g_regex_match (is_number, query, 0, NULL);
}

static void
on_contact_selected (GtkListBox      *box,
                     GtkListBoxRow   *row,
                     ValentSmsWindow *self)
{
  const char *number;

  number = valent_contact_row_get_number (VALENT_CONTACT_ROW (row));
  g_debug ("NUMBER SELECTED: %s", number);
}

static void
on_contact_search_changed (GtkSearchEntry  *entry,
                           ValentSmsWindow *self)
{
  const char *query;

  query = gtk_editable_get_text (GTK_EDITABLE (entry));

  /* If the entry contains a possible phone number... */
  if (check_number (query))
    {
      EContact *contact;
      g_autofree char *name_label = NULL;

      name_label = g_strdup_printf (_("Send to %s"), query);

      /* ...ensure we have a dynamic contact for it */
      if (self->dynamic_contact == NULL)
        {
          /* Create a dummy contact */
          contact = e_contact_new ();
          e_contact_set (contact, E_CONTACT_FULL_NAME, query);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, query);

          /* Create and add a new row */
          self->dynamic_contact = g_object_new (VALENT_TYPE_CONTACT_ROW,
                                                "name",    name_label,
                                                "number",  query,
                                                "contact", contact,
                                                NULL);

          gtk_list_box_insert (self->contact_search_list,
                               self->dynamic_contact,
                               -1);
        }

      /* ...or if we already do, then update it */
      else
        {
          g_object_get (self->dynamic_contact, "contact", &contact, NULL);

          /* Update contact */
          e_contact_set (contact, E_CONTACT_FULL_NAME, query);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, query);

          /* Update row */
          g_object_set (self->dynamic_contact,
                        "name",   name_label,
                        "number", query,
                        NULL);
        }

      /* Drop the extra ref */
      g_clear_pointer (&contact, g_object_unref);
    }

  /* ...otherwise remove the dynamic row if created */
  else if (self->dynamic_contact != NULL)
    {
      gtk_list_box_remove (self->contact_search_list, self->dynamic_contact);
      self->dynamic_contact = NULL;
    }

  gtk_list_box_invalidate_filter (self->contact_search_list);
  gtk_list_box_invalidate_sort (self->contact_search_list);
  gtk_list_box_invalidate_headers (self->contact_search_list);
}

static gboolean
contact_search_list_filter (ValentContactRow *row,
                            ValentSmsWindow  *self)
{
  const char *query;
  g_autofree char *contact_name = NULL;
  g_autofree char *query_name = NULL;

  /* Always show dynamic contact */
  if G_UNLIKELY (GTK_WIDGET (row) == self->dynamic_contact)
    return TRUE;

  query = gtk_editable_get_text (GTK_EDITABLE (self->contact_search_entry));

  if (g_strcmp0 (query, "") == 0)
    return TRUE;

  query_name = g_utf8_casefold (query, -1);
  contact_name = g_utf8_casefold (valent_contact_row_get_name (row), -1);

  /* Show contact if text is substring of name */
  if (g_strrstr (contact_name, query_name) != NULL)
    return TRUE;

  /* Show contact if text is substring of number */
  if (g_strrstr (valent_contact_row_get_number (row), query_name) != NULL)
    return TRUE;

  return FALSE;
}

static gint
contact_search_list_sort (GtkListBoxRow   *row1,
                          GtkListBoxRow   *row2,
                          ValentSmsWindow *self)
{
  g_autofree char *name1 = NULL;
  g_autofree char *name2 = NULL;

  if (GTK_WIDGET (row1) == self->dynamic_contact)
    return -1;

  if (GTK_WIDGET (row2) == self->dynamic_contact)
    return 1;

  g_object_get (row1, "name", &name1, NULL);
  g_object_get (row2, "name", &name2, NULL);

  return g_utf8_collate (name1, name2);
}

static void
refresh_contacts_cb (ValentContactStore *store,
                     GAsyncResult       *result,
                     ValentSmsWindow    *self)
{
  g_autoslist (GObject) contacts = NULL;
  g_autoptr (GError) error = NULL;

  contacts = valent_contact_store_query_finish (store, result, &error);

  if (error != NULL)
    {
      g_warning ("loading contacts: %s", error->message);
      return;
    }

  for (const GSList *iter = contacts; iter; iter = iter->next)
    valent_list_add_contact (self->contact_search_list, iter->data);
}

static void
valent_sms_window_refresh_contacts (ValentSmsWindow *self)
{
  GtkWidget *row;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  /* Clear the list */
  while ((row = gtk_widget_get_first_child (GTK_WIDGET (self->contact_search_list))))
    gtk_list_box_remove (self->contact_search_list, row);

  if (self->contact_store == NULL)
    return;

  /* Add the contacts */
  query = e_book_query_vcard_field_exists (EVC_TEL);
  sexp = e_book_query_to_string (query);

  valent_contact_store_query (self->contact_store,
                              sexp,
                              NULL,
                              (GAsyncReadyCallback)refresh_contacts_cb,
                              self);
}

/*
 * Conversation List
 */
static gboolean
on_send_message (GObject          *object,
                 GList            *participants,
                 ValentSmsMessage *message,
                 ValentSmsWindow  *window)
{
  gboolean message_sent;

  g_signal_emit (G_OBJECT (window),
                 signals [SEND_MESSAGE], 0,
                 participants, message,
                 &message_sent);

  return message_sent;
}

/*
 * Conversation List Callbacks
 */
static GtkWidget *
conversation_list_create (gpointer item,
                          gpointer user_data)
{
  ValentSmsWindow *window = VALENT_SMS_WINDOW (user_data);
  ValentSmsMessage *message = VALENT_SMS_MESSAGE (item);
  GtkWidget *row;
  GVariant *metadata;
  g_autoptr (GVariant) addresses = NULL;

  row = g_object_new (VALENT_TYPE_MESSAGE_ROW,
                      "message", message,
                      NULL);

  metadata = valent_sms_message_get_metadata (message);

  if (g_variant_lookup (metadata, "addresses", "@aa{sv}", &addresses))
    {
      g_autoptr (GVariant) participant = NULL;
      const char *address;

      participant = g_variant_get_child_value (addresses, 0);

      if (g_variant_lookup (participant, "address", "&s", &address))
        valent_contact_store_dup_for_phone_async (window->contact_store,
                                                  address,
                                                  NULL,
                                                  (GAsyncReadyCallback)phone_lookup_cb,
                                                  row);
    }

  return row;
}

static void
conversation_list_populate (ValentSmsWindow *window)
{
  g_autoptr (GListModel) threads = NULL;

  g_assert (VALENT_IS_SMS_WINDOW (window));

  threads = valent_sms_store_get_summary (window->message_store);
  gtk_list_box_bind_model (window->conversation_list,
                           threads,
                           conversation_list_create,
                           window,
                           NULL);
}

static GtkWidget *
valent_sms_window_ensure_conversation (ValentSmsWindow *window,
                                       gint64           thread_id)
{
  GtkWidget *conversation;
  g_autofree char *page_name = NULL;

  page_name = g_strdup_printf ("%li", thread_id);
  conversation = gtk_stack_get_child_by_name (window->content, page_name);

  if (conversation == NULL)
    {
      conversation = g_object_new (VALENT_TYPE_SMS_CONVERSATION,
                                   "contact-store", window->contact_store,
                                   "message-store", window->message_store,
                                   "thread-id",     thread_id,
                                   NULL);

      g_object_bind_property (window,       "contact-store",
                              conversation, "contact-store",
                              G_BINDING_DEFAULT);

      g_signal_connect (G_OBJECT (conversation),
                        "send-message",
                        G_CALLBACK (on_send_message),
                        window);

      gtk_stack_add_named (window->content, conversation, page_name);
    }

  return conversation;
}

static void
on_conversation_activated (GtkListBox      *box,
                           GtkListBoxRow   *row,
                           ValentSmsWindow *self)
{
  ValentMessageRow *summary;
  gint64 thread_id;

  /* Deselect */
  if (row == NULL)
    return;

  summary = VALENT_MESSAGE_ROW (row);
  thread_id = valent_message_row_get_thread_id (summary);

  valent_sms_window_set_active_thread (self, thread_id);
  adw_leaflet_navigate (self->content_box, ADW_NAVIGATION_DIRECTION_FORWARD);
}

static void
on_message_removed (ValentSmsWindow  *window,
                    gint64            thread_id,
                    ValentSmsMessage *message,
                    ValentSmsStore   *model)
{
  GtkWidget *conversation;
  g_autofree char *thread_str = NULL;

  g_assert (VALENT_IS_SMS_WINDOW (window));

  /* If @message is %NULL, the thread is being removed, which is what we're
   * interested in */
  if (message != NULL)
    return;

  /* The GListModel handles the sidebar, so we only destroy the conversation */
  thread_str = g_strdup_printf ("%li", thread_id);
  conversation = gtk_stack_get_child_by_name (window->content, thread_str);

  if (conversation != NULL)
    gtk_stack_remove (window->content, conversation);
}

/*
 * GActions
 */
static void
new_action (GSimpleAction *action,
            GVariant      *parameter,
            gpointer       user_data)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (user_data);

  g_assert (VALENT_IS_SMS_WINDOW (self));

  gtk_list_box_select_row (self->conversation_list, NULL);
  gtk_label_set_label (self->content_title, _("New Conversation"));
  gtk_stack_set_visible_child_name (self->content, "contacts");
  gtk_widget_grab_focus (GTK_WIDGET (self->contact_search_entry));
  adw_leaflet_navigate (self->content_box, ADW_NAVIGATION_DIRECTION_FORWARD);
}

static void
previous_action (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (user_data);

  g_assert (VALENT_IS_SMS_WINDOW (self));

  adw_leaflet_navigate (self->content_box, ADW_NAVIGATION_DIRECTION_BACK);
}

static void
search_action (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (user_data);

  g_assert (VALENT_IS_SMS_WINDOW (self));

  gtk_label_set_label (self->content_title, _("Search Messages"));
  gtk_stack_set_visible_child_name (self->content, "search");
  gtk_widget_grab_focus (GTK_WIDGET (self->message_search_entry));
  adw_leaflet_navigate (self->content_box, ADW_NAVIGATION_DIRECTION_FORWARD);
}

static const GActionEntry actions[] = {
    {"new",      new_action,      NULL, NULL, NULL},
    {"previous", previous_action, NULL, NULL, NULL},
    {"search",   search_action,   NULL, NULL, NULL}
};


/*
 * GObject
 */
static void
valent_sms_window_constructed (GObject *object)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (object);

  g_assert (VALENT_IS_CONTACT_STORE (self->contact_store));
  g_assert (VALENT_IS_SMS_STORE (self->message_store));

  /* Prepare conversation summaries */
  conversation_list_populate (self);
  g_signal_connect_swapped (self->message_store,
                            "message-removed",
                            G_CALLBACK (on_message_removed),
                            self);

  G_OBJECT_CLASS (valent_sms_window_parent_class)->constructed (object);
}

static void
valent_sms_window_finalize (GObject *object)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (object);

  g_signal_handlers_disconnect_by_func (self->message_store,
                                        on_message_removed,
                                        self);

  g_clear_object (&self->contact_store);
  g_clear_object (&self->message_store);

  G_OBJECT_CLASS (valent_sms_window_parent_class)->finalize (object);
}

static void
valent_sms_window_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CONTACT_STORE:
      g_value_set_object (value, self->contact_store);
      break;

    case PROP_MESSAGE_STORE:
      g_value_set_object (value, self->message_store);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_window_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (object);

  switch (prop_id)
    {
    case PROP_CONTACT_STORE:
      valent_sms_window_set_contact_store (self, g_value_get_object (value));
      break;

    case PROP_MESSAGE_STORE:
      g_set_object (&self->message_store, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_sms_window_class_init (ValentSmsWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_sms_window_constructed;
  object_class->finalize = valent_sms_window_finalize;
  object_class->get_property = valent_sms_window_get_property;
  object_class->set_property = valent_sms_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/sms/valent-sms-window.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, content_box);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, content_header);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, sidebar_header);

  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, content_title);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, content_layout);


  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, conversation_list);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, content);

  gtk_widget_class_bind_template_callback (widget_class, on_conversation_activated);

  /* Message Search */
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, message_search);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, message_search_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, message_search_list);
  gtk_widget_class_bind_template_callback (widget_class, on_message_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_message_selected);

  /* Contact Search */
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, contact_search);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, contact_search_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentSmsWindow, contact_search_list);
  gtk_widget_class_bind_template_callback (widget_class, on_contact_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_contact_selected);

  /**
   * ValentSmsWindow:contact-store:
   *
   * The #ValentContactStore providing contacts for the window.
   */
  properties [PROP_CONTACT_STORE] =
    g_param_spec_object ("contact-store",
                         "Contact Store",
                         "The contact model for this window.",
                         VALENT_TYPE_CONTACT_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentSmsWindow:message-store:
   *
   * The #ValentSmsStore providing messages for the window.
   */
  properties [PROP_MESSAGE_STORE] =
    g_param_spec_object ("message-store",
                         "Message Store",
                         "The message store for this window.",
                         VALENT_TYPE_SMS_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentSmsWindow::send-message:
   * @window: a #ValentSmsWindow
   * @destination: a #GList
   * @message: a #ValentSmsMessage
   *
   * The #ValentSmsWindow::send-message signal is emitted when a child
   * #ValentSmsConversation emits #ValentSmsConversation::send-message.
   *
   * @destination is a #GList of target addresses and @message is a #ValentSmsMessage
   * containing the content to be sent.
   *
   * The signal handler should return a boolean indicating success, although
   * this is only used as an indication that the message was sent, not received
   * on the other end.
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
valent_sms_window_init (ValentSmsWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Window Actions */
  g_action_map_add_action_entries (G_ACTION_MAP (self),
                                   actions, G_N_ELEMENTS (actions),
                                   self);

  gtk_list_box_set_header_func (self->message_search_list,
                                search_header_func,
                                self, NULL);

  /* Contacts */
  gtk_list_box_set_filter_func (self->contact_search_list,
                                (GtkListBoxFilterFunc)contact_search_list_filter,
                                self,
                                NULL);

  gtk_list_box_set_sort_func (self->contact_search_list,
                              (GtkListBoxSortFunc)contact_search_list_sort,
                              self,
                              NULL);

  gtk_list_box_set_header_func (self->contact_search_list,
                                valent_contact_row_header_func,
                                self,
                                NULL);
}

/**
 * valent_sms_window_get_contact_store:
 * @window: a #ValentSmsWindow
 *
 * Get the #ValentContactStore providing contacts for @window.
 *
 * Returns: (transfer none) (nullable): a #ValentContactStore
 */
ValentContactStore *
valent_sms_window_get_contact_store (ValentSmsWindow *window)
{
  g_return_val_if_fail (VALENT_IS_SMS_WINDOW (window), NULL);

  return window->contact_store;
}

/**
 * valent_sms_window_set_contact_store:
 * @window: a #ValentSmsWindow
 * @store: a #ValentContactStore
 *
 * Set the #ValentContactStore providing contacts for @window.
 */
void
valent_sms_window_set_contact_store (ValentSmsWindow    *window,
                                     ValentContactStore *store)
{
  if (g_set_object (&window->contact_store, store))
    g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_CONTACT_STORE]);

  valent_sms_window_refresh_contacts (window);
}

/**
 * valent_sms_window_get_sms_store:
 * @window: a #ValentSmsWindow
 *
 * Get the #ValentSmsStore providing messages for @window.
 *
 * Returns: (transfer none) (nullable): a #ValentSmsStore
 */
ValentSmsStore *
valent_sms_window_get_sms_store (ValentSmsWindow *window)
{
  g_return_val_if_fail (VALENT_IS_SMS_WINDOW (window), NULL);

  return window->message_store;
}

/**
 * valent_sms_window_set_active_message:
 * @window: a #ValentSmsWindow
 * @message: a #valentMessage
 *
 * Set the active conversation to the thread of @message scroll to @message.
 */
void
valent_sms_window_set_active_message (ValentSmsWindow  *window,
                                      ValentSmsMessage *message)
{
  GtkWidget *widget;
  ValentSmsConversation *conversation;
  gint64 thread_id;

  g_return_if_fail (VALENT_IS_SMS_WINDOW (window));

  /* Select the conversation */
  thread_id = valent_sms_message_get_thread_id (message);
  valent_sms_window_set_active_thread (window, thread_id);

  /* Get the conversation */
  widget = valent_sms_window_ensure_conversation (window, thread_id);
  conversation = VALENT_SMS_CONVERSATION (widget);

  valent_sms_conversation_scroll_to_message (conversation, message);
}

/**
 * valent_sms_window_set_active_thread:
 * @window: a #ValentSmsWindow
 * @thread_id: a thread id
 *
 * Set the active conversation
 */
void
valent_sms_window_set_active_thread (ValentSmsWindow *window,
                                     gint64           thread_id)
{
  GtkWidget *conversation;
  const char *title;

  g_return_if_fail (VALENT_IS_SMS_WINDOW (window));
  g_return_if_fail (thread_id > 0);

  /* Ensure a conversation widget exists */
  conversation = valent_sms_window_ensure_conversation (window, thread_id);

  /* Set the header title */
  title = valent_sms_conversation_get_title (VALENT_SMS_CONVERSATION (conversation));
  gtk_label_set_label (window->content_title, title);

  /* Switch to conversation widget */
  gtk_stack_set_visible_child (window->content, conversation);
}

