// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-page"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>
#include <valent.h>

#include "valent-contact-row.h"

#include "valent-contact-page.h"


struct _ValentContactPage
{
  AdwNavigationPage  parent_instance;

  GListModel        *contacts;
  GtkWidget         *placeholder_contact;
  char              *search_query;

  /* template */
  GtkWidget         *search_entry;
  GtkListBox        *contact_list;
  GListModel        *model;
  GtkFilter         *filter;
  GtkStringSorter   *sorter;

  AdwDialog         *details_dialog;
  GtkListBox        *medium_list;
};

G_DEFINE_FINAL_TYPE (ValentContactPage, valent_contact_page, ADW_TYPE_NAVIGATION_PAGE)

typedef enum {
  PROP_CONTACTS = 1,
} ValentContactPageProperty;

static GParamSpec *properties[PROP_CONTACTS + 1] = { NULL, };

typedef enum {
  SELECTED,
} ValentContactPageSignal;

static guint signals[SELECTED + 1] = { 0, };

static char *
_phone_number_normalize (const char *number)
{
  g_autofree char *normalized = NULL;
  const char *s = number;
  size_t i = 0;

  g_assert (number != NULL);

  normalized = g_new (char, strlen (number) + 1);
  while (*s != '\0')
    {
      if G_LIKELY (g_ascii_isdigit (*s))
        normalized[i++] = *s;

      s++;
    }
  normalized[i] = '\0';
  normalized = g_realloc (normalized, i * sizeof (char));

  return g_steal_pointer (&normalized);
}

static gboolean
_e_contact_has_number (EContact   *contact,
                       const char *query)
{
  GStrv tel_normalized = NULL;
  gboolean ret = FALSE;

  tel_normalized = g_object_get_data (G_OBJECT (contact), "tel-normalized");
  if (tel_normalized == NULL)
    {
      g_autoptr (GStrvBuilder) builder = NULL;
      GList *numbers = NULL;

      builder = g_strv_builder_new ();
      numbers = e_contact_get (contact, E_CONTACT_TEL);
      for (const GList *iter = numbers; iter != NULL; iter = iter->next)
        g_strv_builder_take (builder, _phone_number_normalize (iter->data));
      g_list_free_full (numbers, g_free);

      tel_normalized = g_strv_builder_end (builder);
      g_object_set_data_full (G_OBJECT (contact),
                              "tel-normalized",
                              tel_normalized,
                              (GDestroyNotify)g_strfreev);
    }

  for (size_t i = 0; tel_normalized != NULL && tel_normalized[i] != NULL; i++)
    {
      ret = strstr (tel_normalized[i], query) != NULL;
      if (ret)
        break;
    }

  return ret;
}

static inline gboolean
valent_contact_page_filter (gpointer item,
                            gpointer user_data)
{
  EContact *contact = E_CONTACT (item);
  ValentContactPage *self = VALENT_CONTACT_PAGE (user_data);
  g_autolist (EVCardAttribute) attrs = NULL;
  const char *query;
  g_autofree char *query_folded = NULL;
  g_autofree char *name = NULL;

  attrs = e_contact_get (contact, E_CONTACT_TEL);
  if (attrs == NULL)
    return FALSE;

  query = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  if (g_strcmp0 (query, "") == 0)
    return TRUE;

  /* Show contact if text is substring of name
   */
  query_folded = g_utf8_casefold (query, -1);
  name = g_utf8_casefold (e_contact_get_const (contact, E_CONTACT_FULL_NAME), -1);
  if (g_strrstr (name, query_folded) != NULL)
    return TRUE;

  if (_e_contact_has_number (contact, query))
    return TRUE;

  return FALSE;
}

static void
on_search_changed (GtkSearchEntry    *entry,
                   ValentContactPage *self)
{
  const char *query;

  query = gtk_editable_get_text (GTK_EDITABLE (entry));
#if 0
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
#endif

  if (self->search_query && g_str_has_prefix (query, self->search_query))
    gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_MORE_STRICT);
  else if (self->search_query && g_str_has_prefix (self->search_query, query))
    gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_LESS_STRICT);
  else
    gtk_filter_changed (self->filter, GTK_FILTER_CHANGE_DIFFERENT);

  g_set_str (&self->search_query, query);
}

static GtkWidget *
contact_list_create (gpointer item,
                     gpointer user_data)
{
  EContact *contact = E_CONTACT (item);
  GtkWidget *row;
  g_autolist (EVCardAttribute) attrs = NULL;
  g_autofree char *number = NULL;
  unsigned int n_attrs;

  attrs = e_contact_get_attributes (contact, E_CONTACT_TEL);
  n_attrs = g_list_length (attrs);

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

  return row;
}

static void
on_contact_medium_selected (AdwActionRow      *row,
                            ValentContactPage *self)
{
  EContact *contact;
  const char *medium;

  g_assert (ADW_IS_ACTION_ROW (row));

  contact = g_object_get_data (G_OBJECT (row), "contact");
  medium = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (row));
  g_signal_emit (G_OBJECT (self), signals [SELECTED], 0, contact, medium);

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
on_contact_selected (ValentContactPage *self)
{
  g_autolist (EVCardAttribute) attrs = NULL;
  GtkListBoxRow *row;
  EContact *contact;

  g_assert (VALENT_IS_CONTACT_PAGE (self));

  row = gtk_list_box_get_selected_row (GTK_LIST_BOX (self->contact_list));
  if (row == NULL)
    return;

  contact = valent_contact_row_get_contact (VALENT_CONTACT_ROW (row));
  attrs = e_contact_get_attributes (E_CONTACT (contact), E_CONTACT_TEL);

  if (g_list_length (attrs) == 1)
    {
      g_autofree char *medium = NULL;

      g_object_get (row, "contact-medium", &medium, NULL);
      g_signal_emit (G_OBJECT (self), signals [SELECTED], 0, contact, medium);
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

  g_clear_object (&self->contacts);
  g_clear_pointer (&self->search_query, g_free);

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
    case PROP_CONTACTS:
      g_value_set_object (value, self->contacts);
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
    case PROP_CONTACTS:
      g_set_object (&self->contacts, g_value_get_object (value));
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
  gtk_widget_class_bind_template_child (widget_class, ValentContactPage, details_dialog);
  gtk_widget_class_bind_template_child (widget_class, ValentContactPage, medium_list);
  gtk_widget_class_bind_template_child (widget_class, ValentContactPage, model);
  gtk_widget_class_bind_template_child (widget_class, ValentContactPage, filter);
  gtk_widget_class_bind_template_child (widget_class, ValentContactPage, sorter);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_contact_selected);

  page_class->shown = valent_contact_page_shown;

  /**
   * ValentContactPage:contacts:
   *
   * The `ValentContactsAdapter` providing contacts.
   */
  properties [PROP_CONTACTS] =
    g_param_spec_object ("contacts", NULL, NULL,
                         VALENT_TYPE_CONTACTS_ADAPTER,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
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

  gtk_custom_filter_set_filter_func (GTK_CUSTOM_FILTER (self->filter),
                                     valent_contact_page_filter,
                                     self, NULL);
  gtk_list_box_bind_model (self->contact_list,
                           self->model,
                           contact_list_create,
                           self, NULL);
}

