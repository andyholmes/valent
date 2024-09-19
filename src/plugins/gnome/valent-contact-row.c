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
#include "valent-ui-utils-private.h"


struct _ValentContactRow
{
  GtkListBoxRow  parent_instance;

  EContact      *contact;

  GtkWidget     *grid;
  GtkWidget     *avatar;
  GtkWidget     *title_label;
  GtkWidget     *subtitle_label;
  GtkWidget     *type_label;
};

G_DEFINE_FINAL_TYPE (ValentContactRow, valent_contact_row, GTK_TYPE_LIST_BOX_ROW)

typedef enum {
  PROP_CONTACT = 1,
  PROP_CONTACT_MEDIUM,
  PROP_CONTACT_TYPE,
} ValentContactRowProperty;

static GParamSpec *properties[PROP_CONTACT_TYPE + 1] = { NULL, };

static void
valent_contact_row_set_compact (ValentContactRow *row,
                                gboolean          compact)
{
  g_return_if_fail (VALENT_IS_CONTACT_ROW (row));

  if (compact)
    {
      gtk_widget_set_visible (row->title_label, FALSE);
      gtk_widget_set_visible (row->avatar, FALSE);
    }
  else
    {
      gtk_widget_set_visible (row->title_label, TRUE);
      gtk_widget_set_visible (row->avatar, TRUE);
    }
}

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

  switch ((ValentContactRowProperty)prop_id)
    {
    case PROP_CONTACT:
      g_value_set_object (value, valent_contact_row_get_contact (self));
      break;

    case PROP_CONTACT_MEDIUM:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (self->subtitle_label)));
      break;

    case PROP_CONTACT_TYPE:
      g_value_set_string (value, gtk_label_get_text (GTK_LABEL (self->type_label)));
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

  switch ((ValentContactRowProperty)prop_id)
    {
    case PROP_CONTACT:
      valent_contact_row_set_contact (self, g_value_get_object (value));
      break;

    case PROP_CONTACT_MEDIUM:
      gtk_label_set_text (GTK_LABEL (self->subtitle_label), g_value_get_string (value));
      break;

    case PROP_CONTACT_TYPE:
      gtk_label_set_text (GTK_LABEL (self->type_label), g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_contact_row_class_init (ValentContactRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_contact_row_finalize;
  object_class->get_property = valent_contact_row_get_property;
  object_class->set_property = valent_contact_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-contact-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentContactRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, ValentContactRow, title_label);
  gtk_widget_class_bind_template_child (widget_class, ValentContactRow, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, ValentContactRow, type_label);

  properties [PROP_CONTACT] =
    g_param_spec_object ("contact", NULL, NULL,
                         E_TYPE_CONTACT,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_CONTACT_MEDIUM] =
    g_param_spec_string ("contact-medium", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_CONTACT_TYPE] =
    g_param_spec_string ("contact-type", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_contact_row_init (ValentContactRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * valent_contact_row_header_func:
 * @row: a `GtkListBoxRow`
 * @before: (nullable): a `GtkListBoxRow`
 * @user_data: user supplied data
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
      gtk_widget_add_css_class (label, "caption-heading");
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
        {
          valent_contact_row_set_compact (contact_row, TRUE);
          gtk_widget_set_margin_bottom (GTK_WIDGET (before), 0);
        }
      else
        {
          valent_contact_row_set_compact (contact_row, FALSE);
          gtk_widget_set_margin_bottom (GTK_WIDGET (before), 6);
        }
    }
}

/**
 * valent_contact_row_sort_func:
 * @row1: the first row
 * @row2: the second row
 * @user_data: user supplied data
 *
 * A `GtkListBoxSortFunc` for `ValentContactRow` widgets that sorts
 * alphabetically by the title label.
 *
 * Returns: `< 0` if @row1 should be before @row2 , `0` if they are equal
 *   and `> 0` otherwise
 */
int
valent_contact_row_sort_func (GtkListBoxRow *row1,
                              GtkListBoxRow *row2,
                              gpointer       user_data)
{
  GtkListBoxRow *reserved = GTK_LIST_BOX_ROW (user_data);
  ValentContactRow *crow1 = VALENT_CONTACT_ROW (row1);
  ValentContactRow *crow2 = VALENT_CONTACT_ROW (row2);

  if G_UNLIKELY (row1 == reserved)
    return -1;

  if G_UNLIKELY (row2 == reserved)
    return 1;

  return g_utf8_collate (gtk_label_get_label (GTK_LABEL (crow1->title_label)),
                         gtk_label_get_label (GTK_LABEL (crow2->title_label)));
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
      const char *type_ = NULL;

      number = e_vcard_attribute_get_value (attr);

      row = g_object_new (VALENT_TYPE_CONTACT_ROW,
                          "contact", contact,
                          NULL);

      /* NOTE: the ordering below results in a preference of Work, Mobile, Home.
       * Justification being that Work is more important context than mobility,
       * while mobility is more relevant than Home if the number is personal. */
      if (e_vcard_attribute_has_type (attr, "WORK"))
        type_ = _("Work");
      else if (e_vcard_attribute_has_type (attr, "CELL"))
        type_ = _("Mobile");
      else if (e_vcard_attribute_has_type (attr, "HOME"))
        type_ = _("Home");
      else
        type_ = _("Other");

      gtk_label_set_label (GTK_LABEL (row->subtitle_label), number);
      gtk_label_set_label (GTK_LABEL (row->type_label), type_);

      gtk_list_box_insert (list, GTK_WIDGET (row), -1);
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
  gtk_label_set_label (GTK_LABEL (row->title_label), name);
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
}

/**
 * valent_contact_row_get_contact_medium:
 * @row: a `ValentContactRow`
 *
 * Get the contact medium displayed in @row.
 *
 * Returns: (transfer none): a phone number string
 */
const char *
valent_contact_row_get_contact_medium (ValentContactRow *row)
{
  g_return_val_if_fail (VALENT_IS_CONTACT_ROW (row), NULL);

  return gtk_label_get_text (GTK_LABEL (row->subtitle_label));
}

