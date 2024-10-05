// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-messages-window"

#include "config.h"

#include <inttypes.h>

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>
#include <valent.h>

#include "valent-contact-page.h"
#include "valent-contact-row.h"
#include "valent-conversation-page.h"
#include "valent-ui-utils-private.h"
#include "valent-message-row.h"

#include "valent-messages-window.h"


struct _ValentMessagesWindow
{
  AdwApplicationWindow    parent_instance;

  GListModel             *contacts;
  ValentContactsAdapter  *contacts_adapter;
  GListModel             *messages;
  ValentMessagesAdapter  *messages_adapter;
  GCancellable           *search;

  /* template */
  AdwNavigationSplitView *main_view;
  AdwHeaderBar           *sidebar_header;
  GtkListBox             *sidebar_list;
  AdwNavigationView      *content_view;
  AdwNavigationPage      *search_page;
  GtkWidget              *search_entry;
  GtkListBox             *search_list;
  AdwNavigationPage      *contact_page;
  AdwDialog              *details_dialog;
  GtkListBox             *medium_list;
};

void   valent_messages_window_set_active_message (ValentMessagesWindow *window,
                                                  ValentMessage        *message);
void   valent_messages_window_set_active_thread  (ValentMessagesWindow *window,
                                                  const char           *iri);

G_DEFINE_FINAL_TYPE (ValentMessagesWindow, valent_messages_window, ADW_TYPE_APPLICATION_WINDOW)

typedef enum {
  PROP_MESSAGES = 1,
} ValentMessagesWindowProperty;

static GParamSpec *properties[PROP_MESSAGES + 1] = { NULL, };


/*
 * Contact Lookup
 */
static void
lookup_contact_cb (ValentContactsAdapter *adapter,
                   GAsyncResult          *result,
                   ValentMessageRow      *row)
{
  g_autoptr (EContact) contact = NULL;
  g_autoptr (GError) error = NULL;

  contact = valent_contacts_adapter_reverse_lookup_finish (adapter, result, &error);
  if (contact == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  valent_message_row_set_contact (row, contact);
}

static void
search_contacts_cb (ValentContactsAdapter *adapter,
                    GAsyncResult          *result,
                    ValentMessagesWindow  *self)
{
  g_autoptr (GListModel) contacts = NULL;
  unsigned int n_contacts;
  g_autoptr (GError) error = NULL;

  contacts = valent_contacts_adapter_search_finish (adapter, result, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  n_contacts = g_list_model_get_n_items (contacts);
  for (unsigned int i = 0; i < n_contacts; i++)
    {
      g_autoptr (EContact) contact = g_list_model_get_item (contacts, i);
      GtkWidget *row;
      g_autolist (EVCardAttribute) attrs = NULL;
      g_autofree char *number = NULL;
      unsigned int n_attrs;

      attrs = e_contact_get_attributes (contact, E_CONTACT_TEL);
      n_attrs = g_list_length (attrs);
      if (n_attrs == 0)
        continue;

      g_object_get (contact, "primary-phone", &number, NULL);
      if (number == NULL || *number == '\0')
        {
          g_free (number);
          number = e_vcard_attribute_get_value ((EVCardAttribute *)attrs->data);
        }

      if (n_attrs > 1)
        {
          g_autofree char *tmp = g_steal_pointer (&number);

          number = g_strdup_printf (ngettext ("%s and %u more…",
                                              "%s and %u more…",
                                              n_attrs - 1),
                                    tmp, n_attrs - 1);
        }

      row = g_object_new (VALENT_TYPE_CONTACT_ROW,
                          "contact",        contact,
                          "contact-medium", number,
                          NULL);

      if (n_attrs > 1)
        {
          gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                                       GTK_ACCESSIBLE_STATE_EXPANDED, FALSE,
                                       -1);
        }

      gtk_list_box_insert (self->search_list, row, -1);
    }
}

static void
search_messages_cb (ValentMessagesAdapter *adapter,
                    GAsyncResult          *result,
                    ValentMessagesWindow  *self)
{
  g_autoptr (GListModel) messages = NULL;
  unsigned int n_messages;
  g_autoptr (GError) error = NULL;

  messages = valent_messages_adapter_search_finish (adapter, result, &error);
  if (messages == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  n_messages = g_list_model_get_n_items (messages);
  for (unsigned int i = 0; i < n_messages; i++)
    {
      g_autoptr (ValentMessage) message = g_list_model_get_item (messages, i);
      GtkWidget *row;
      const char *medium;

      row = g_object_new (VALENT_TYPE_MESSAGE_ROW,
                          "message", message,
                          NULL);
      gtk_list_box_insert (self->search_list, row, -1);

      medium = valent_message_get_sender (message);
      if (medium == NULL || *medium == '\0')
        {
          const char * const *recipients = NULL;

          recipients = valent_message_get_recipients (message);
          if (recipients != NULL)
            medium = recipients[0];
        }

      if (medium != NULL && *medium != '\0')
        {
          g_autoptr (GCancellable) cancellable = NULL;

          cancellable = g_cancellable_new ();
          g_signal_connect_object (row,
                                   "destroy",
                                   G_CALLBACK (g_cancellable_cancel),
                                   cancellable,
                                   G_CONNECT_SWAPPED);
          valent_contacts_adapter_reverse_lookup (self->contacts_adapter,
                                                  medium,
                                                  cancellable,
                                                  (GAsyncReadyCallback) lookup_contact_cb,
                                                  row);
        }
    }
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
                                "margin-bottom", 6,
                                "margin-end",    6,
                                "margin-start",  6,
                                "margin-top",    6,
                                NULL);
          gtk_widget_add_css_class (label, "dim-label");
          gtk_widget_add_css_class (label, "caption-heading");
          gtk_list_box_row_set_header (row, label);
        }
    }
  else if (!VALENT_IS_CONTACT_ROW (before))
    {
      GtkWidget *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "label",        _("Contacts"),
                            "halign",       GTK_ALIGN_START,
                            "margin-bottom", 6,
                            "margin-end",    6,
                            "margin-start",  6,
                            "margin-top",    12, // +6 for section spacing
                            NULL);
      gtk_widget_add_css_class (label, "dim-label");
      gtk_widget_add_css_class (label, "caption-heading");
      gtk_list_box_row_set_header (row, label);
    }
}

static void
on_search_changed (GtkSearchEntry       *entry,
                   ValentMessagesWindow *self)
{
  GtkWidget *child;
  const char *search_query;

  /* Clear previous results
   */
  g_cancellable_cancel (self->search);
  g_clear_object (&self->search);

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->search_list))))
    gtk_list_box_remove (self->search_list, child);

  search_query = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (search_query == NULL || *search_query == '\0')
    return;

  /* Search messages
   */
  self->search = g_cancellable_new ();
  valent_messages_adapter_search (self->messages_adapter,
                                  search_query,
                                  self->search,
                                  (GAsyncReadyCallback)search_messages_cb,
                                  self);
  valent_contacts_adapter_search (self->contacts_adapter,
                                  search_query,
                                  self->search,
                                  (GAsyncReadyCallback)search_contacts_cb,
                                  self);
}

static void
lookup_thread_cb (ValentMessagesAdapter *adapter,
                  GAsyncResult          *result,
                  ValentMessagesWindow  *self)
{
  g_autofree char *iri = NULL;
  g_autoptr (GError) error = NULL;

  iri = g_task_propagate_pointer (G_TASK (result), &error);
  if (iri == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);
      else
        g_debug ("%s(): no thread found for contact", G_STRFUNC);

      return;
    }

  adw_navigation_view_pop (self->content_view);
  valent_messages_window_set_active_thread (self, iri);
}

static void
on_contact_selected (ValentContactPage    *page,
                     EContact             *contact,
                     const char           *target,
                     ValentMessagesWindow *self)
{
  g_autoptr (GCancellable) cancellable = NULL;

  cancellable = g_cancellable_new ();
  g_signal_connect_object (self,
                           "destroy",
                           G_CALLBACK (g_cancellable_cancel),
                           cancellable,
                           G_CONNECT_SWAPPED);

  valent_messages_adapter_lookup_thread (self->messages_adapter,
                                         ((const char * const []){ target, NULL }),
                                         cancellable,
                                         (GAsyncReadyCallback) lookup_thread_cb,
                                         self);

  // FIXME: loading indicator
}

static void
on_contact_medium_selected (AdwActionRow         *row,
                            ValentMessagesWindow *self)
{
  g_autoptr (GCancellable) cancellable = NULL;
  const char *medium;

  g_assert (ADW_IS_ACTION_ROW (row));

  cancellable = g_cancellable_new ();
  g_signal_connect_object (self,
                           "destroy",
                           G_CALLBACK (g_cancellable_cancel),
                           cancellable,
                           G_CONNECT_SWAPPED);

  medium = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  valent_messages_adapter_lookup_thread (self->messages_adapter,
                                         ((const char * const []){ medium, NULL }),
                                         cancellable,
                                         (GAsyncReadyCallback) lookup_thread_cb,
                                         self);

  adw_dialog_close (self->details_dialog);
}

static void
on_contact_row_collapsed (AdwDialog *dialog,
                          GtkWidget *row)
{
  gtk_accessible_reset_relation (GTK_ACCESSIBLE (row),
                                 GTK_ACCESSIBLE_RELATION_CONTROLS);
  gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                               GTK_ACCESSIBLE_STATE_EXPANDED, FALSE,
                               -1);
  g_signal_handlers_disconnect_by_func (dialog, on_contact_row_collapsed, row);
}

static void
on_search_selected (GtkListBox           *box,
                    GtkListBoxRow        *row,
                    ValentMessagesWindow *self)
{
  g_assert (VALENT_IS_MESSAGES_WINDOW (self));

  if (VALENT_IS_MESSAGE_ROW (row))
    {
      ValentMessage *message;

      message = valent_message_row_get_message (VALENT_MESSAGE_ROW (row));
      valent_messages_window_set_active_message (self, message);

      /* Reset the search
       */
      adw_navigation_view_pop (self->content_view);
      gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
    }
  else if (VALENT_IS_CONTACT_ROW (row))
    {
      g_autolist (EVCardAttribute) attrs = NULL;
      EContact *contact;

      contact = valent_contact_row_get_contact (VALENT_CONTACT_ROW (row));
      attrs = e_contact_get_attributes (E_CONTACT (contact), E_CONTACT_TEL);

      if (g_list_length (attrs) == 1)
        {
          g_autofree char *medium = NULL;

          g_object_get (row, "contact-medium", &medium, NULL);
          on_contact_medium_selected (ADW_ACTION_ROW (row), self);

          /* Reset the search
           */
          adw_navigation_view_pop (self->content_view);
          gtk_editable_set_text (GTK_EDITABLE (self->search_entry), "");
        }
      else
        {
          gtk_list_box_remove_all (GTK_LIST_BOX (self->medium_list));
          for (const GList *iter = attrs; iter; iter = iter->next)
            {
              EVCardAttribute *attr = iter->data;
              GtkWidget *medium_row;
              g_autofree char *number = NULL;
              const char *type_ = NULL;

              if (e_vcard_attribute_has_type (attr, "WORK"))
                type_ = _("Work");
              else if (e_vcard_attribute_has_type (attr, "CELL"))
                type_ = _("Mobile");
              else if (e_vcard_attribute_has_type (attr, "HOME"))
                type_ = _("Home");
              else
                type_ = _("Other");

              number = e_vcard_attribute_get_value (attr);
              medium_row = g_object_new (ADW_TYPE_ACTION_ROW,
                                         "activatable", TRUE,
                                         "title",       number,
                                         "subtitle",    type_,
                                         NULL);
              g_object_set_data_full (G_OBJECT (medium_row),
                                      "contact",
                                      g_object_ref (contact),
                                      g_object_unref);
              g_signal_connect_object (medium_row,
                                       "activated",
                                       G_CALLBACK (on_contact_medium_selected),
                                       self,
                                       G_CONNECT_DEFAULT);

              gtk_list_box_insert (self->medium_list, medium_row, -1);
            }

          /* Present the dialog and match the expanded state
           */
          gtk_accessible_update_state (GTK_ACCESSIBLE (row),
                                       GTK_ACCESSIBLE_STATE_EXPANDED, TRUE,
                                       -1);
          gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                          GTK_ACCESSIBLE_RELATION_CONTROLS, self->details_dialog, NULL,
                                          -1);
          g_signal_connect_object (self->details_dialog,
                                   "closed",
                                   G_CALLBACK (on_contact_row_collapsed),
                                   row,
                                   G_CONNECT_DEFAULT);
          adw_dialog_present (self->details_dialog, GTK_WIDGET (self));
        }
    }
}

/*
 * Sidebar
 */
static GtkWidget *
sidebar_list_create (gpointer item,
                     gpointer user_data)
{
  ValentMessagesWindow *window = VALENT_MESSAGES_WINDOW (user_data);
  GListModel *thread = G_LIST_MODEL (item);
  g_autoptr (ValentMessage) message = NULL;
  ValentMessageBox box;
  const char * const *recipients = NULL;
  const char *medium = NULL;
  GtkWidget *row;

  g_object_get (thread, "latest-message", &message, NULL);
  row = g_object_new (VALENT_TYPE_MESSAGE_ROW,
                      "message", message,
                      NULL);
  g_object_bind_property (thread, "latest-message",
                          row,    "message",
                          G_BINDING_DEFAULT);
  g_object_set_data_full (G_OBJECT (row),
                          "valent-message-thread",
                          g_object_ref (thread),
                          g_object_unref);

  // TODO: participant-based avatar for sidebar rows
  box = valent_message_get_box (message);
  if (box == VALENT_MESSAGE_BOX_INBOX)
    {
      medium = valent_message_get_sender (message);
    }
  else if (box == VALENT_MESSAGE_BOX_SENT)
    {
      recipients = valent_message_get_recipients (message);
      if (recipients != NULL)
        medium = recipients[0];
    }

  if (window->contacts_adapter != NULL &&
      medium != NULL && *medium != '\0')
    {
      g_autoptr (GCancellable) cancellable = NULL;

      cancellable = g_cancellable_new ();
      g_signal_connect_object (row,
                               "destroy",
                               G_CALLBACK (g_cancellable_cancel),
                               cancellable,
                               G_CONNECT_SWAPPED);
      valent_contacts_adapter_reverse_lookup (window->contacts_adapter,
                                              medium,
                                              cancellable,
                                              (GAsyncReadyCallback) lookup_contact_cb,
                                              row);
    }
  else
    {
      g_autoptr (EContact) contact = NULL;

      contact = e_contact_new ();
      e_contact_set (contact, E_CONTACT_FULL_NAME, _("Unknown"));
      e_contact_set (contact, E_CONTACT_PHONE_OTHER, _("Unknown sender"));
      valent_message_row_set_contact (VALENT_MESSAGE_ROW (row), contact);
    }

  return row;
}

static GtkWidget *
valent_messages_window_ensure_conversation (ValentMessagesWindow *window,
                                            const char           *thread_iri)
{
  AdwNavigationPage *conversation;

  conversation = adw_navigation_view_find_page (window->content_view, thread_iri);
  if (conversation == NULL)
    {
      conversation = g_object_new (VALENT_TYPE_CONVERSATION_PAGE,
                                   "tag",      thread_iri,
                                   "contacts", window->contacts_adapter,
                                   "messages", window->messages_adapter,
                                   "iri",      thread_iri,
                                   NULL);
      adw_navigation_view_push (window->content_view, conversation);
    }
  else
    {
      adw_navigation_view_pop_to_page (window->content_view, conversation);
    }

  return GTK_WIDGET (conversation);
}

static void
on_conversation_activated (GtkListBox           *box,
                           GtkListBoxRow        *row,
                           ValentMessagesWindow *self)
{
  ValentContext *context;
  GtkWidget *page;
  EContact *contact;
  ValentMessage *message;
  const char *sender;
  g_autofree char *iri = NULL;

  g_assert (VALENT_IS_MESSAGES_WINDOW (self));
  g_assert (VALENT_IS_MESSAGE_ROW (row));

  if (row == NULL)
    return;

  // TODO: use IRI
  contact = valent_message_row_get_contact (VALENT_MESSAGE_ROW (row));
  message = valent_message_row_get_message (VALENT_MESSAGE_ROW (row));
  context = valent_extension_get_context (VALENT_EXTENSION (self->messages_adapter));
  iri = g_strdup_printf ("valent://%s/%"PRId64,
                         valent_context_get_path (context),
                         valent_message_get_thread_id (message));

  page = valent_messages_window_ensure_conversation (self, iri);
  sender = valent_message_get_sender (message);
  if (sender != NULL && *sender != '\0')
    {
      valent_conversation_page_add_participant (VALENT_CONVERSATION_PAGE (page),
                                                contact,
                                                sender);
    }

  adw_navigation_split_view_set_show_content (self->main_view, TRUE);
}

/*
 * Message Source
 */
static void
on_selected_item (GObject              *object,
                  GParamSpec           *pspec,
                  ValentMessagesWindow *self)
{
  ValentMessagesAdapter *adapter = NULL;
  GObject *owner = NULL;
  unsigned int n_items = 0;

  g_assert (VALENT_IS_MESSAGES_WINDOW (self));

  adapter = gtk_drop_down_get_selected_item (GTK_DROP_DOWN (object));
  if (!g_set_object (&self->messages_adapter, adapter))
    return;

  // FIXME: adapters need properties
  if (g_strcmp0 (G_OBJECT_TYPE_NAME (adapter), "ValentSmsDevice") != 0)
    {
      g_warning ("%s(): unsupported message source \"%s\"",
                 G_STRFUNC,
                 G_OBJECT_TYPE_NAME (adapter));
      return;
    }

  // HACK: try to find a matching contacts adapter
  owner = valent_extension_get_object (VALENT_EXTENSION (adapter));
  n_items = g_list_model_get_n_items (self->contacts);
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (ValentContactsAdapter) item = NULL;
      GObject *item_owner = NULL;

      item = g_list_model_get_item (self->contacts, i);
      item_owner = valent_extension_get_object (VALENT_EXTENSION (item));
      if (item_owner == owner)
        {
          g_set_object (&self->contacts_adapter, item);
          break;
        }
    }

  gtk_list_box_bind_model (self->sidebar_list,
                           G_LIST_MODEL (adapter),
                           sidebar_list_create,
                           self,
                           NULL);
}

/*
 * GActions
 */
static void
sms_new_action (GtkWidget  *widget,
                const char *action_name,
                GVariant   *parameter)
{
  ValentMessagesWindow *self = VALENT_MESSAGES_WINDOW (widget);

  g_assert (VALENT_IS_MESSAGES_WINDOW (self));

  gtk_list_box_select_row (self->sidebar_list, NULL);

  if (self->contact_page == NULL)
    {
      self->contact_page = g_object_new (VALENT_TYPE_CONTACT_PAGE,
                                         "tag",      "contacts",
                                         "contacts", self->contacts_adapter,
                                         NULL);
      g_signal_connect_object (self->contact_page,
                               "selected",
                               G_CALLBACK (on_contact_selected),
                               self,
                               G_CONNECT_DEFAULT);
      adw_navigation_view_push (self->content_view, self->contact_page);
    }
  else
    {
      adw_navigation_view_pop_to_page (self->content_view, self->contact_page);
    }

  adw_navigation_split_view_set_show_content (self->main_view, TRUE);
}

static void
sms_search_action (GtkWidget  *widget,
                   const char *action_name,
                   GVariant   *parameter)
{
  ValentMessagesWindow *self = VALENT_MESSAGES_WINDOW (widget);

  g_assert (VALENT_IS_MESSAGES_WINDOW (self));

  if (self->search_page == NULL)
    adw_navigation_view_push_by_tag (self->content_view, "search");
  else
    adw_navigation_view_pop_to_page (self->content_view, self->search_page);

  gtk_widget_grab_focus (self->search_entry);
  adw_navigation_split_view_set_show_content (self->main_view, TRUE);
}

/*
 * AdwNavigationPage
 */
static void
on_page_popped (AdwNavigationView    *view,
                AdwNavigationPage    *page,
                ValentMessagesWindow *self)
{
  if (self->contact_page == page)
    self->contact_page = NULL;
  else if (self->search_page == page)
    self->search_page = NULL;
}

static void
on_page_pushed (AdwNavigationView    *view,
                ValentMessagesWindow *self)
{
  AdwNavigationPage *page = adw_navigation_view_get_visible_page (view);
  const char *tag = adw_navigation_page_get_tag (page);

  if (g_strcmp0 (tag, "contacts") == 0)
    self->contact_page = page;
  else if (g_strcmp0 (tag, "search") == 0)
    self->search_page = page;
}

/*
 * GObject
 */
static void
valent_messages_window_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_MESSAGES_WINDOW);

  G_OBJECT_CLASS (valent_messages_window_parent_class)->dispose (object);
}

static void
valent_messages_window_finalize (GObject *object)
{
  ValentMessagesWindow *self = VALENT_MESSAGES_WINDOW (object);

  g_clear_object (&self->contacts);
  g_clear_object (&self->contacts_adapter);
  g_clear_object (&self->messages);
  g_clear_object (&self->messages_adapter);

  G_OBJECT_CLASS (valent_messages_window_parent_class)->finalize (object);
}

static void
valent_messages_window_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ValentMessagesWindow *self = VALENT_MESSAGES_WINDOW (object);

  switch (prop_id)
    {
    case PROP_MESSAGES:
      g_value_set_object (value, self->messages);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_messages_window_class_init (ValentMessagesWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_messages_window_dispose;
  object_class->finalize = valent_messages_window_finalize;
  object_class->get_property = valent_messages_window_get_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-messages-window.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, main_view);
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, sidebar_header);
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, sidebar_list);
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, content_view);
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, search_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, search_list);
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, details_dialog);
  gtk_widget_class_bind_template_child (widget_class, ValentMessagesWindow, medium_list);
  gtk_widget_class_bind_template_callback (widget_class, on_conversation_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_page_popped);
  gtk_widget_class_bind_template_callback (widget_class, on_page_pushed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_contact_selected);
  gtk_widget_class_bind_template_callback (widget_class, on_search_selected);
  gtk_widget_class_bind_template_callback (widget_class, on_selected_item);

  gtk_widget_class_install_action (widget_class, "sms.new", NULL, sms_new_action);
  gtk_widget_class_install_action (widget_class, "sms.search", NULL, sms_search_action);

  properties [PROP_MESSAGES] =
    g_param_spec_object ("messages", NULL, NULL,
                         VALENT_TYPE_MESSAGES,
                         (G_PARAM_READABLE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  g_type_ensure (VALENT_TYPE_CONTACT_PAGE);
}

static void
valent_messages_window_init (ValentMessagesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_set_object (&self->contacts, G_LIST_MODEL (valent_contacts_get_default ()));
  g_set_object (&self->messages, G_LIST_MODEL (valent_messages_get_default ()));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MESSAGES]);

  gtk_list_box_set_header_func (self->search_list,
                                search_header_func,
                                self, NULL);
}

/*< private >
 * valent_messages_window_set_active_message:
 * @window: a `ValentMessagesWindow`
 * @message: a `ValentMessage`
 *
 * Set the active conversation to the thread of @message scroll to @message.
 */
void
valent_messages_window_set_active_message (ValentMessagesWindow *window,
                                           ValentMessage        *message)
{
  GtkWidget *widget;
  ValentConversationPage *conversation;
  g_autofree char *iri = NULL;
  ValentContext *context;
  int64_t thread_id;

  g_return_if_fail (VALENT_IS_MESSAGES_WINDOW (window));

  context = valent_extension_get_context (VALENT_EXTENSION (window->messages_adapter));
  thread_id = valent_message_get_thread_id (message);
  iri = g_strdup_printf ("valent://%s/%"PRId64,
                         valent_context_get_path (context),
                         thread_id);

  widget = valent_messages_window_ensure_conversation (window, iri);
  conversation = VALENT_CONVERSATION_PAGE (widget);
  valent_conversation_page_scroll_to_message (conversation, message);
}

/**
 * valent_messages_window_set_active_thread:
 * @window: a `ValentMessagesWindow`
 * @iri: a thread IRI
 *
 * Set the active conversation
 */
void
valent_messages_window_set_active_thread (ValentMessagesWindow *window,
                                          const char           *iri)
{
  g_return_if_fail (VALENT_IS_MESSAGES_WINDOW (window));
  g_return_if_fail (iri != NULL);

  valent_messages_window_ensure_conversation (window, iri);
}

