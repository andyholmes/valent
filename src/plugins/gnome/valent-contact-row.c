// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-contact-row"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libebook-contacts/libebook-contacts.h>
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
      g_value_set_object (value, self->contact);
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
  gtk_widget_class_bind_template_callback (widget_class, valent_contact_to_paintable);

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
  g_return_if_fail (VALENT_IS_CONTACT_ROW (row));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (g_set_object (&row->contact, contact))
    g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
}

