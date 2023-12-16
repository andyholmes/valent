// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-row"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

#include "valent-contact-row.h"
#include "valent-sms-utils.h"


struct _ValentContactRow
{
  GtkListBoxRow  parent_instance;

  EContact      *contact;

  GtkWidget     *grid;
  GtkWidget     *avatar;
  GtkWidget     *name_label;
  GtkWidget     *address_label;
  GtkWidget     *address_type_label;
};

G_DEFINE_FINAL_TYPE (ValentContactRow, valent_contact_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_CONTACT,
  PROP_CONTACT_ADDRESS,
  PROP_CONTACT_NAME,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


/*
 * GObject
 */
static void
valent_contact_row_finalize (GObject *object)
{
  ValentContactRow *self = VALENT_CONTACT_ROW (object);

  g_clear_object (&self->contact);

  G_OBJECT_CLASS (valent_contact_row_parent_class)->finalize (object);
}

static void
valent_contact_row_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentContactRow *self = VALENT_CONTACT_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTACT:
      g_value_set_object (value, valent_contact_row_get_contact (self));
      break;

    case PROP_CONTACT_ADDRESS:
      g_value_set_string (value, valent_contact_row_get_contact_address (self));
      break;

    case PROP_CONTACT_NAME:
      g_value_set_string (value, valent_contact_row_get_contact_name (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentContactRow *self = VALENT_CONTACT_ROW (object);

  switch (prop_id)
    {
    case PROP_CONTACT:
      valent_contact_row_set_contact (self, g_value_get_object (value));
      break;

    case PROP_CONTACT_ADDRESS:
      valent_contact_row_set_contact_address (self, g_value_get_string (value));
      break;

    case PROP_CONTACT_NAME:
      valent_contact_row_set_contact_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_row_class_init (ValentContactRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_contact_row_finalize;
  object_class->get_property = valent_contact_row_get_property;
  object_class->set_property = valent_contact_row_set_property;

  /**
   * ValentContactRow:contact
   *
   * The `EContact` for this row.
   */
  properties [PROP_CONTACT] =
    g_param_spec_object ("contact", NULL, NULL,
                         E_TYPE_CONTACT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentContactRow:contact-address
   *
   * The phone number, e-mail or other address format for the contact.
   *
   * Usually this will be a phone number, however SMS messages may originate
   * from an SMS gateway service. In this case the address may be in another
   * format.
   */
  properties [PROP_CONTACT_ADDRESS] =
    g_param_spec_string ("contact-address", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentContactRow:contact-name
   *
   * The contact name displayed in the row, by default the full name of
   * `ValentContactRow`:contact.
   */
  properties [PROP_CONTACT_NAME] =
    g_param_spec_string ("contact-name", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_contact_row_init (ValentContactRow *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self), "valent-contact-row");

  self->grid = g_object_new (GTK_TYPE_GRID,
                             "column-spacing", 8,
                             "margin-start",   8,
                             "margin-end",     8,
                             "margin-top",     6,
                             "margin-bottom",  6,
                             NULL);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (self), self->grid);

  self->avatar = g_object_new (ADW_TYPE_AVATAR,
                               "size",    32,
                               "halign",  GTK_ALIGN_START,
                               "valign",  GTK_ALIGN_CENTER,
                               "vexpand", TRUE,
                               NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->avatar, 0, 0, 1, 2);

  self->name_label = g_object_new (GTK_TYPE_LABEL,
                                   "halign",  GTK_ALIGN_START,
                                   "hexpand", TRUE,
                                   "valign",  GTK_ALIGN_CENTER,
                                   "vexpand", TRUE,
                                   "xalign",  0.0,
                                   NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->name_label, 1, 0, 2, 1);

  self->address_label = g_object_new (GTK_TYPE_LABEL,
                                      "ellipsize", PANGO_ELLIPSIZE_END,
                                      "halign",    GTK_ALIGN_START,
                                      "hexpand",   TRUE,
                                      "valign",    GTK_ALIGN_CENTER,
                                      "vexpand",   TRUE,
                                      "xalign",    0.0,
                                      NULL);
  gtk_widget_add_css_class (self->address_label, "dim-label");
  gtk_grid_attach (GTK_GRID (self->grid), self->address_label, 1, 1, 1, 1);

  self->address_type_label = g_object_new (GTK_TYPE_LABEL,
                                   "label",     _("Other"),
                                   "ellipsize", PANGO_ELLIPSIZE_END,
                                   "halign",    GTK_ALIGN_END,
                                   "hexpand",   FALSE,
                                   "valign",    GTK_ALIGN_CENTER,
                                   "vexpand",   TRUE,
                                   "xalign",    0.0,
                                   NULL);
  gtk_widget_add_css_class (self->address_type_label, "dim-label");
  gtk_grid_attach (GTK_GRID (self->grid), self->address_type_label, 2, 1, 1, 1);
}

/**
 * valent_contact_row_header_func:
 * @row: a `GtkListBoxRow`
 * @before: (nullable): a `GtkListBoxRow`
 * @user_data: (closure): user supplied data
 *
 * A `GtkListBoxHeaderFunc` for `ValentContactRow` widgets that takes care of
 * hiding or showing the avatar and name depending on whether the row is
 * grouped with other rows for the same contact.
 *
 * For example, if @before is not a `ValentContactRow` or for a different `EContact`
 * the avatar and name will be shown, otherwise it's considered a secondary row.
 */
void
valent_contact_row_header_func (GtkListBoxRow *row,
                                GtkListBoxRow *before,
                                gpointer       user_data)
{
  ValentContactRow *contact_row;

  if G_UNLIKELY (!VALENT_IS_CONTACT_ROW (row))
    return;

  contact_row = VALENT_CONTACT_ROW (row);

  if (before == NULL)
    {
      valent_contact_row_set_compact (contact_row, FALSE);
    }
  else if (!VALENT_IS_CONTACT_ROW (before))
    {
      GtkWidget *label;

      label = g_object_new (GTK_TYPE_LABEL,
                            "label",        _("Contacts"),
                            "halign",       GTK_ALIGN_START,
                            "margin-end",   6,
                            "margin-start", 6,
                            "margin-top",   6,
                            NULL);
      gtk_widget_add_css_class (label, "dim-label");
      gtk_widget_add_css_class (label, "list-header-title");
      gtk_list_box_row_set_header (row, label);

      valent_contact_row_set_compact (contact_row, FALSE);
    }
  else
    {
      EContact *row_contact;
      EContact *before_contact;

      row_contact = valent_contact_row_get_contact (contact_row);
      before_contact = valent_contact_row_get_contact (VALENT_CONTACT_ROW (before));

      if (g_strcmp0 (e_contact_get_const (row_contact, E_CONTACT_UID),
                     e_contact_get_const (before_contact, E_CONTACT_UID)) == 0)
        valent_contact_row_set_compact (contact_row, TRUE);
      else
        valent_contact_row_set_compact (contact_row, FALSE);
    }
}

/**
 * valent_list_add_contact:
 * @list: a `GtkListBox`
 * @contact: an `EContact`
 *
 * A convenience for adding a `ValentContactRow` to @list for each @contact
 * number.
 */
void
valent_list_add_contact (GtkListBox *list,
                         EContact   *contact)
{
  g_autolist (EVCardAttribute) attrs = NULL;

  g_return_if_fail (GTK_IS_LIST_BOX (list));
  g_return_if_fail (E_IS_CONTACT (contact));

  attrs = e_contact_get_attributes (contact, E_CONTACT_TEL);

  for (const GList *iter = attrs; iter; iter = iter->next)
    {
      EVCardAttribute *attr = iter->data;
      ValentContactRow *row;
      g_autofree char *number = NULL;
      const char *type = NULL;

      number = e_vcard_attribute_get_value (attr);

      row = g_object_new (VALENT_TYPE_CONTACT_ROW,
                          "contact", contact,
                          NULL);

      /* NOTE: the ordering below results in a preference of Work, Mobile, Home.
       * Justification being that Work is more important context than mobility,
       * while mobility is more relevant than Home if the number is personal. */
      if (e_vcard_attribute_has_type (attr, "WORK"))
        type = _("Work");

      else if (e_vcard_attribute_has_type (attr, "CELL"))
        type = _("Mobile");

      else if (e_vcard_attribute_has_type (attr, "HOME"))
        type = _("Home");

      else
        type = _("Other");

      gtk_label_set_label (GTK_LABEL (row->address_label), number);
      gtk_label_set_label (GTK_LABEL (row->address_type_label), type);

      gtk_list_box_insert (list, GTK_WIDGET (row), -1);
    }
}

/**
 * valent_contact_row_new:
 * @contact: an `EContact`
 *
 * Create a new `ValentContactRow` for @contact.
 *
 * Returns: (transfer full): a new `ValentContactRow`
 */
GtkWidget *
valent_contact_row_new (EContact *contact)
{
  return g_object_new (VALENT_TYPE_CONTACT_ROW,
                       "contact", contact,
                       NULL);
}

/**
 * valent_contact_row_set_compact:
 * @row: a `ValentContactRow`
 * @compact: %TRUE or %FALSE
 *
 * Set whether @row should display the name and avatar (%FALSE) or not (%TRUE).
 */
void
valent_contact_row_set_compact (ValentContactRow *row,
                                gboolean          compact)
{
  g_return_if_fail (VALENT_IS_CONTACT_ROW (row));

  if (compact)
    {
      gtk_widget_set_visible (row->name_label, FALSE);
      gtk_widget_set_visible (row->avatar, FALSE);
      // avatar (32px) + column spacing (8px)
      gtk_widget_set_margin_start (row->grid, 48);
    }
  else
    {
      gtk_widget_set_visible (row->name_label, TRUE);
      gtk_widget_set_visible (row->avatar, TRUE);
      gtk_widget_set_margin_start (row->grid, 8);
    }
}

/**
 * valent_contact_row_get_contact:
 * @row: a `ValentContactRow`
 *
 * Get the `EContact` for @row.
 *
 * Returns: (transfer none): a `EContact`
 */
EContact *
valent_contact_row_get_contact (ValentContactRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_ROW (row), NULL);

  return row->contact;
}

/**
 * valent_contact_row_set_contact:
 * @row: a `ValentContactRow`
 * @contact: a `ValentContact`
 *
 * Set the `ValentContact` for @row.
 */
void
valent_contact_row_set_contact (ValentContactRow *row,
                                EContact         *contact)
{
  const char *name = NULL;

  g_return_if_fail (VALENT_IS_CONTACT_ROW (row));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (!g_set_object (&row->contact, contact))
    return;

  if (row->contact != NULL)
    {
      name = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
      valent_sms_avatar_from_contact (ADW_AVATAR (row->avatar), contact);
    }

  valent_contact_row_set_compact (row, FALSE);
  valent_contact_row_set_contact_name (row, name);
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
}

/**
 * valent_contact_row_get_contact_address:
 * @row: a `ValentContactRow`
 *
 * Get the contact address displayed in @row.
 *
 * Returns: (transfer none): a phone number string
 */
const char *
valent_contact_row_get_contact_address (ValentContactRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_ROW (row), NULL);

  return gtk_label_get_text (GTK_LABEL (row->address_label));
}

/**
 * valent_contact_row_set_contact_address:
 * @row: a `ValentContactRow`
 * @address: a phone number or other address
 *
 * Set the contact address displayed in @row.
 */
void
valent_contact_row_set_contact_address (ValentContactRow *row,
                                        const char       *address)
{
  g_return_if_fail (VALENT_IS_CONTACT_ROW (row));

  gtk_label_set_text (GTK_LABEL (row->address_label), address);
  gtk_label_set_text (GTK_LABEL (row->address_type_label), _("Other"));
}

/**
 * valent_contact_row_get_contact_name:
 * @row: a `ValentContactRow`
 *
 * Get the contact name displayed in @row.
 *
 * Returns: (transfer none): a contact name
 */
const char *
valent_contact_row_get_contact_name (ValentContactRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_ROW (row), NULL);

  return gtk_label_get_text (GTK_LABEL (row->name_label));
}

/**
 * valent_contact_row_set_contact_name:
 * @row: a `ValentContactRow`
 * @name: a contact name
 *
 * Set the contact name displayed in @row.
 */
void
valent_contact_row_set_contact_name (ValentContactRow *row,
                                     const char       *name)
{
  g_return_if_fail (VALENT_IS_CONTACT_ROW (row));

  if (name == NULL && E_IS_CONTACT (row->contact))
    name = e_contact_get_const (row->contact, E_CONTACT_FULL_NAME);

  gtk_label_set_text (GTK_LABEL (row->name_label), name);
}

