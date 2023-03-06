// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-sms-window"

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <valent.h>

#include "valent-contact-row.h"
#include "valent-message.h"
#include "valent-sms-conversation.h"
#include "valent-sms-store.h"
#include "valent-sms-utils.h"
#include "valent-sms-window.h"
#include "valent-message-row.h"


struct _ValentSmsWindow
{
  AdwApplicationWindow  parent_instance;

  ValentContactStore   *contact_store;
  ValentSmsStore       *message_store;

  /* template */
  AdwLeaflet           *content_box;
  AdwHeaderBar         *content_header;
  AdwHeaderBar         *sidebar_header;

  GtkLabel             *content_title;
  GtkBox               *content_layout;
  GtkListBox           *conversation_list;
  GtkStack             *content;

  GtkWidget            *message_search;
  GtkWidget            *message_search_entry;
  GtkListBox           *message_search_list;

  GtkWidget            *contact_search;
  GtkWidget            *contact_search_entry;
  GtkListBox           *contact_search_list;
  GtkWidget            *placeholder_contact;
};

G_DEFINE_FINAL_TYPE (ValentSmsWindow, valent_sms_window, ADW_TYPE_APPLICATION_WINDOW)

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
phone_lookup_cb (ValentContactStore *store,
                 GAsyncResult       *result,
                 ValentMessageRow   *row)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (EContact) contact = NULL;

  contact = valent_sms_contact_from_phone_finish (store, result, &error);

  if (contact == NULL)
      g_warning ("%s(): %s", G_STRFUNC, error->message);

  valent_message_row_set_contact (row, contact);
}

static void
search_contacts_cb (ValentContactStore *model,
                    GAsyncResult       *result,
                    ValentSmsWindow    *window)
{
  g_autoptr (GError) error = NULL;
  g_autoslist (GObject) contacts = NULL;

  contacts = valent_contact_store_query_finish (model, result, &error);

  for (const GSList *iter = contacts; iter; iter = iter->next)
    valent_list_add_contact (window->contact_search_list, iter->data);

  if (error != NULL)
    g_warning ("%s(): %s", G_STRFUNC, error->message);
}

static void
search_messages_cb (ValentSmsStore  *store,
                    GAsyncResult    *result,
                    ValentSmsWindow *window)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GPtrArray) messages = NULL;

  messages = valent_sms_store_find_messages_finish (store, result, &error);

  for (unsigned int i = 0; i < messages->len; i++)
    {
      ValentMessage *message;
      GtkWidget *row;
      const char *address;

      message = g_ptr_array_index (messages, i);

      row = g_object_new (VALENT_TYPE_MESSAGE_ROW,
                          "message", message,
                          NULL);
      gtk_list_box_insert (window->message_search_list, row, -1);

      if ((address = valent_message_get_sender (message)) == NULL)
        {
          GVariant *metadata;
          g_autoptr (GVariant) addresses = NULL;
          g_autoptr (GVariant) address_dict = NULL;

          metadata = valent_message_get_metadata (message);

          if (!g_variant_lookup (metadata, "addresses", "@aa{sv}", &addresses))
            continue;

          if (g_variant_n_children (addresses) == 0)
            continue;

          address_dict = g_variant_get_child_value (addresses, 0);

          if (!g_variant_lookup (address_dict, "address", "&s", &address))
            continue;
        }

      valent_sms_contact_from_phone (window->contact_store,
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

          label = g_object_new (GTK_TYPE_LABEL,
                                "label",        _("Conversations"),
                                "halign",       GTK_ALIGN_START,
                                "margin-end",   6,
                                "margin-start", 6,
                                "margin-top",   6,
                                NULL);
          gtk_widget_add_css_class (label, "dim-label");
          gtk_widget_add_css_class (label, "list-header-title");
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
  valent_sms_store_find_messages (window->message_store,
                                  query_str,
                                  NULL,
                                  (GAsyncReadyCallback)search_messages_cb,
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
                              (GAsyncReadyCallback)search_contacts_cb,
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
      ValentMessage *message;

      message = valent_message_row_get_message (VALENT_MESSAGE_ROW (row));
      valent_sms_window_set_active_message (self, message);

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
  const char *address;

  address = valent_contact_row_get_contact_address (VALENT_CONTACT_ROW (row));
  g_debug ("NUMBER SELECTED: %s", address);
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
      if (self->placeholder_contact == NULL)
        {
          /* Create a dummy contact */
          contact = e_contact_new ();
          e_contact_set (contact, E_CONTACT_FULL_NAME, query);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, query);

          /* Create and add a new row */
          self->placeholder_contact = g_object_new (VALENT_TYPE_CONTACT_ROW,
                                                "contact",         contact,
                                                "contact-name",    name_label,
                                                "contact-address", query,
                                                NULL);

          gtk_list_box_insert (self->contact_search_list,
                               self->placeholder_contact,
                               -1);
        }

      /* ...or if we already do, then update it */
      else
        {
          g_object_get (self->placeholder_contact, "contact", &contact, NULL);

          /* Update contact */
          e_contact_set (contact, E_CONTACT_FULL_NAME, query);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, query);

          /* Update row */
          g_object_set (self->placeholder_contact,
                        "contact-name",    name_label,
                        "contact-address", query,
                        NULL);
        }

      /* Drop the extra ref */
      g_clear_pointer (&contact, g_object_unref);
    }

  /* ...otherwise remove the dynamic row if created */
  else if (self->placeholder_contact != NULL)
    {
      gtk_list_box_remove (self->contact_search_list,
                           self->placeholder_contact);
      self->placeholder_contact = NULL;
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
  g_autofree char *query_folded = NULL;
  g_autofree char *name = NULL;
  const char *address = NULL;

  /* Always show dynamic contact */
  if G_UNLIKELY (GTK_WIDGET (row) == self->placeholder_contact)
    return TRUE;

  query = gtk_editable_get_text (GTK_EDITABLE (self->contact_search_entry));

  if (g_strcmp0 (query, "") == 0)
    return TRUE;

  query_folded = g_utf8_casefold (query, -1);

  /* Show contact if text is substring of name */
  name = g_utf8_casefold (valent_contact_row_get_contact_name (row), -1);

  if (g_strrstr (name, query_folded) != NULL)
    return TRUE;

  /* Show contact if text is substring of number */
  address = valent_contact_row_get_contact_address (row);

  if (g_strrstr (address, query_folded))
    return TRUE;

  return FALSE;
}

static gint
contact_search_list_sort (GtkListBoxRow   *row1,
                          GtkListBoxRow   *row2,
                          ValentSmsWindow *self)
{
  const char *name1;
  const char *name2;

  if G_UNLIKELY (GTK_WIDGET (row1) == self->placeholder_contact)
    return -1;

  if G_UNLIKELY (GTK_WIDGET (row2) == self->placeholder_contact)
    return 1;

  name1 = valent_contact_row_get_contact_name (VALENT_CONTACT_ROW (row1));
  name2 = valent_contact_row_get_contact_name (VALENT_CONTACT_ROW (row2));

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
      g_warning ("%s(): %s", G_STRFUNC, error->message);
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
on_send_message (GObject         *object,
                 ValentMessage   *message,
                 ValentSmsWindow *window)
{
  gboolean sent;

  g_signal_emit (G_OBJECT (window), signals [SEND_MESSAGE], 0, message, &sent);

  return sent;
}

/*
 * Conversation List Callbacks
 */
static GtkWidget *
conversation_list_create (gpointer item,
                          gpointer user_data)
{
  ValentSmsWindow *window = VALENT_SMS_WINDOW (user_data);
  ValentMessage *message = VALENT_MESSAGE (item);
  GtkWidget *row;
  GVariant *metadata;
  g_autoptr (GVariant) addresses = NULL;

  row = g_object_new (VALENT_TYPE_MESSAGE_ROW,
                      "message", message,
                      NULL);

  /* TODO: probably a failure of kdeconnect-android, but occasionally a message
   *       will have no addresses */
  if ((metadata = valent_message_get_metadata (message)) != NULL &&
      g_variant_lookup (metadata, "addresses", "@aa{sv}", &addresses) &&
      g_variant_n_children (addresses) > 0)
    {
      g_autoptr (GVariant) participant = NULL;
      const char *address;

      participant = g_variant_get_child_value (addresses, 0);

      if (g_variant_lookup (participant, "address", "&s", &address))
        {
          valent_sms_contact_from_phone (window->contact_store,
                                         address,
                                         NULL,
                                         (GAsyncReadyCallback)phone_lookup_cb,
                                         row);
        }
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

  page_name = g_strdup_printf ("%"G_GINT64_FORMAT, thread_id);
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
  gtk_widget_grab_focus (self->contact_search_entry);
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
  gtk_widget_grab_focus (self->message_search_entry);
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

  G_OBJECT_CLASS (valent_sms_window_parent_class)->constructed (object);
}

static void
valent_sms_window_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_SMS_WINDOW);

  G_OBJECT_CLASS (valent_sms_window_parent_class)->dispose (object);
}

static void
valent_sms_window_finalize (GObject *object)
{
  ValentSmsWindow *self = VALENT_SMS_WINDOW (object);

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
  object_class->dispose = valent_sms_window_dispose;
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
    g_param_spec_object ("contact-store", NULL, NULL,
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
    g_param_spec_object ("message-store", NULL, NULL,
                         VALENT_TYPE_SMS_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentSmsWindow::send-message:
   * @window: a #ValentSmsWindow
   * @message: a #ValentMessage
   *
   * The #ValentSmsWindow::send-message signal is emitted when a child
   * #ValentSmsConversation emits #ValentSmsConversation::send-message.
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
  g_return_if_fail (VALENT_IS_SMS_WINDOW (window));
  g_return_if_fail (store == NULL || VALENT_IS_CONTACT_STORE (store));

  if (!g_set_object (&window->contact_store, store))
    return;

  valent_sms_window_refresh_contacts (window);
  g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_CONTACT_STORE]);
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
valent_sms_window_get_message_store (ValentSmsWindow *window)
{
  g_return_val_if_fail (VALENT_IS_SMS_WINDOW (window), NULL);

  return window->message_store;
}

/**
 * valent_sms_window_search_contacts:
 * @window: a #ValentSmsWindow
 * @query: query string
 *
 * Switch the contact view and search for @query.
 */
void
valent_sms_window_search_contacts (ValentSmsWindow *window,
                                   const char      *query)
{
  g_return_if_fail (VALENT_IS_SMS_WINDOW (window));
  g_return_if_fail (query != NULL);

  gtk_list_box_select_row (window->conversation_list, NULL);

  gtk_label_set_label (window->content_title, _("New Conversation"));
  gtk_stack_set_visible_child_name (window->content, "contacts");
  gtk_widget_grab_focus (window->contact_search_entry);
  adw_leaflet_navigate (window->content_box, ADW_NAVIGATION_DIRECTION_FORWARD);

  gtk_editable_set_text (GTK_EDITABLE (window->contact_search_entry), query);
}

/**
 * valent_sms_window_search_messages:
 * @window: a #ValentSmsWindow
 * @query: query string
 *
 * Switch the search view and search for @query.
 */
void
valent_sms_window_search_messages (ValentSmsWindow *window,
                                   const char      *query)
{
  g_return_if_fail (VALENT_IS_SMS_WINDOW (window));
  g_return_if_fail (query != NULL);

  gtk_label_set_label (window->content_title, _("Search Messages"));
  gtk_stack_set_visible_child_name (window->content, "search");
  gtk_widget_grab_focus (window->message_search_entry);
  adw_leaflet_navigate (window->content_box, ADW_NAVIGATION_DIRECTION_FORWARD);

  gtk_editable_set_text (GTK_EDITABLE (window->message_search_entry), query);
}

/**
 * valent_sms_window_set_active_message:
 * @window: a #ValentSmsWindow
 * @message: a #valentMessage
 *
 * Set the active conversation to the thread of @message scroll to @message.
 */
void
valent_sms_window_set_active_message (ValentSmsWindow *window,
                                      ValentMessage   *message)
{
  GtkWidget *widget;
  ValentSmsConversation *conversation;
  gint64 thread_id;

  g_return_if_fail (VALENT_IS_SMS_WINDOW (window));

  /* Select the conversation */
  thread_id = valent_message_get_thread_id (message);
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
  g_return_if_fail (thread_id >= 0);

  /* Ensure a conversation widget exists */
  conversation = valent_sms_window_ensure_conversation (window, thread_id);

  /* Set the header title */
  title = valent_sms_conversation_get_title (VALENT_SMS_CONVERSATION (conversation));
  gtk_label_set_label (window->content_title, title);

  /* Switch to conversation widget */
  gtk_stack_set_visible_child (window->content, conversation);
}

