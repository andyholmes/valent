// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-page"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-contact-row.h"

#include "valent-contact-page.h"


struct _ValentContactPage
{
  AdwNavigationPage   parent_instance;

  ValentContactStore *contact_store;

  /* template */
  GtkWidget          *search_entry;
  GtkListBox         *contact_list;
  GtkWidget          *placeholder_contact;
};

G_DEFINE_FINAL_TYPE (ValentContactPage, valent_contact_page, ADW_TYPE_NAVIGATION_PAGE)

typedef enum {
  PROP_CONTACT_STORE = 1,
} ValentContactPageProperty;

static GParamSpec *properties[PROP_CONTACT_STORE + 1] = { NULL, };

typedef enum {
  SELECTED,
} ValentContactPageSignal;

static guint signals[SELECTED + 1] = { 0, };


static gboolean
check_number (const char *query)
{
  static GRegex *is_number = NULL;

  if (is_number == NULL)
    is_number = g_regex_new ("(?!0)[\\d]{3,}", G_REGEX_OPTIMIZE, 0, NULL);

  return g_regex_match (is_number, query, 0, NULL);
}

static void
on_search_changed (GtkSearchEntry    *entry,
                   ValentContactPage *self)
{
  const char *query;

  query = gtk_editable_get_text (GTK_EDITABLE (entry));
  if (check_number (query))
    {
      g_autoptr (EContact) contact = NULL;
      g_autofree char *name_label = NULL;

      /* TRANSLATORS: dynamic string for phone number input (e.g. "Send to 911")
       */
      name_label = g_strdup_printf (_("Send to %s"), query);

      if (self->placeholder_contact == NULL)
        {
          contact = e_contact_new ();
          e_contact_set (contact, E_CONTACT_FULL_NAME, query);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, query);

          self->placeholder_contact = g_object_new (VALENT_TYPE_CONTACT_ROW,
                                                    "contact",        contact,
                                                    "contact-name",   name_label,
                                                    "contact-medium", query,
                                                    NULL);
          gtk_list_box_insert (self->contact_list,
                               self->placeholder_contact,
                               -1);
        }
      else
        {
          g_object_get (self->placeholder_contact, "contact", &contact, NULL);

          e_contact_set (contact, E_CONTACT_FULL_NAME, query);
          e_contact_set (contact, E_CONTACT_PHONE_OTHER, query);
          g_object_set (self->placeholder_contact,
                        "contact-name",   name_label,
                        "contact-medium", query,
                        NULL);
        }
    }
  else if (self->placeholder_contact != NULL)
    {
      gtk_list_box_remove (self->contact_list,
                           self->placeholder_contact);
      self->placeholder_contact = NULL;
    }

  gtk_list_box_invalidate_filter (self->contact_list);
  gtk_list_box_invalidate_sort (self->contact_list);
  gtk_list_box_invalidate_headers (self->contact_list);
}

static gboolean
contact_list_filter (ValentContactRow  *row,
                     ValentContactPage *self)
{
  const char *query;
  g_autofree char *query_folded = NULL;
  g_autofree char *name = NULL;
  const char *medium = NULL;
  EContact *contact;

  /* Always show the dynamic contact
   */
  if G_UNLIKELY (GTK_WIDGET (row) == self->placeholder_contact)
    return TRUE;

  query = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  if (g_strcmp0 (query, "") == 0)
    return TRUE;

  /* Show contact if text is substring of name
   */
  query_folded = g_utf8_casefold (query, -1);
  contact = valent_contact_row_get_contact (row);
  name = g_utf8_casefold (e_contact_get_const (contact, E_CONTACT_FULL_NAME), -1);
  if (g_strrstr (name, query_folded) != NULL)
    return TRUE;

  /* Show contact if text is substring of medium
   */
  medium = valent_contact_row_get_contact_medium (row);
  if (g_strrstr (medium, query_folded) != NULL)
    return TRUE;

  return FALSE;
}

static void
refresh_contacts_cb (ValentContactStore *store,
                     GAsyncResult       *result,
                     ValentContactPage  *self)
{
  g_autoslist (GObject) contacts = NULL;
  g_autoptr (GError) error = NULL;

  contacts = valent_contact_store_query_finish (store, result, &error);
  if (error != NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  for (const GSList *iter = contacts; iter; iter = iter->next)
    valent_list_add_contact (self->contact_list, iter->data);
}

static void
valent_contact_page_refresh_contacts (ValentContactPage *self)
{
  GtkWidget *row;
  g_autoptr (EBookQuery) query = NULL;
  g_autofree char *sexp = NULL;

  while ((row = gtk_widget_get_first_child (GTK_WIDGET (self->contact_list))))
    gtk_list_box_remove (self->contact_list, row);

  if (self->contact_store == NULL)
    return;

  query = e_book_query_vcard_field_exists (EVC_TEL);
  sexp = e_book_query_to_string (query);
  valent_contact_store_query (self->contact_store,
                              sexp,
                              NULL,
                              (GAsyncReadyCallback)refresh_contacts_cb,
                              self);
}

static void
on_contact_selected (ValentContactPage *self)
{
  GtkListBoxRow *row;
  EContact *contact;
  const char *medium;

  g_assert (VALENT_IS_CONTACT_PAGE (self));

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->contact_list));
  if (row == NULL)
    return;

  contact = valent_contact_row_get_contact (VALENT_CONTACT_ROW (row));
  medium = valent_contact_row_get_contact_medium (VALENT_CONTACT_ROW (row));

  g_signal_emit (G_OBJECT (self), signals [SELECTED], 0, contact, medium);
}

/*
 * AdwNavigationPage
 */
static void
valent_contact_page_shown (AdwNavigationPage *page)
{
  ValentContactPage *self = VALENT_CONTACT_PAGE (page);

  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));

  if (ADW_NAVIGATION_PAGE_CLASS (valent_contact_page_parent_class)->shown)
    ADW_NAVIGATION_PAGE_CLASS (valent_contact_page_parent_class)->shown (page);
}

/*
 * GObject
 */
static void
valent_contact_page_finalize (GObject *object)
{
  ValentContactPage *self = VALENT_CONTACT_PAGE (object);

  g_clear_object (&self->contact_store);

  G_OBJECT_CLASS (valent_contact_page_parent_class)->finalize (object);
}

static void
valent_contact_page_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentContactPage *self = VALENT_CONTACT_PAGE (object);

  switch ((ValentContactPageProperty)prop_id)
    {
    case PROP_CONTACT_STORE:
      g_value_set_object (value, self->contact_store);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_page_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentContactPage *self = VALENT_CONTACT_PAGE (object);

  switch ((ValentContactPageProperty)prop_id)
    {
    case PROP_CONTACT_STORE:
      valent_contact_page_set_contact_store (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_page_class_init (ValentContactPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  AdwNavigationPageClass *page_class = ADW_NAVIGATION_PAGE_CLASS (klass);

  object_class->finalize = valent_contact_page_finalize;
  object_class->get_property = valent_contact_page_get_property;
  object_class->set_property = valent_contact_page_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-contact-page.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentContactPage, search_entry);
  gtk_widget_class_bind_template_child (widget_class, ValentContactPage, contact_list);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_contact_selected);

  page_class->shown = valent_contact_page_shown;

  /**
   * ValentContactPage:contact-store:
   *
   * The `ValentContactStore` providing contacts.
   */
  properties [PROP_CONTACT_STORE] =
    g_param_spec_object ("contact-store", NULL, NULL,
                         VALENT_TYPE_CONTACT_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  /**
   * ValentContactPage::selected:
   * @conversation: a `ValentContactPage`
   * @contact: an `EContact`
   * @medium: a contact medium
   *
   * The `ValentContactPage`::selected signal is emitted when a contact
   * medium (e.g. phone number, email, etc) is selected.
   */
  signals [SELECTED] =
    g_signal_new ("selected",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, E_TYPE_CONTACT, G_TYPE_STRING);
}

static void
valent_contact_page_init (ValentContactPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_filter_func (self->contact_list,
                                (GtkListBoxFilterFunc)contact_list_filter,
                                self,
                                NULL);
  gtk_list_box_set_sort_func (self->contact_list,
                              valent_contact_row_sort_func,
                              self,
                              NULL);
  gtk_list_box_set_header_func (self->contact_list,
                                valent_contact_row_header_func,
                                self,
                                NULL);
}

/**
 * valent_contact_page_get_contact_store:
 * @window: a `ValentContactPage`
 *
 * Get the `ValentContactStore` providing contacts for @window.
 *
 * Returns: (transfer none) (nullable): a `ValentContactStore`
 */
ValentContactStore *
valent_contact_page_get_contact_store (ValentContactPage *window)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_PAGE (window), NULL);

  return window->contact_store;
}

/**
 * valent_contact_page_set_contact_store:
 * @window: a `ValentContactPage`
 * @store: a `ValentContactStore`
 *
 * Set the `ValentContactStore` providing contacts for @window.
 */
void
valent_contact_page_set_contact_store (ValentContactPage  *window,
                                       ValentContactStore *store)
{
  g_return_if_fail (VALENT_IS_CONTACT_PAGE (window));
  g_return_if_fail (store == NULL || VALENT_IS_CONTACT_STORE (store));

  if (!g_set_object (&window->contact_store, store))
    return;

  valent_contact_page_refresh_contacts (window);
  g_object_notify_by_pspec (G_OBJECT (window), properties[PROP_CONTACT_STORE]);
}

