// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-message-row"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

#include "valent-date-label.h"
#include "valent-ui-utils-private.h"

#include "valent-message-row.h"

struct _ValentMessageRow
{
  GtkListBoxRow  parent_instance;

  ValentMessage *message;
  EContact      *contact;

  /* template */
  GtkWidget     *avatar;
  GtkWidget     *date_label;
  GtkWidget     *title_label;
  GtkWidget     *subtitle_label;
};

G_DEFINE_FINAL_TYPE (ValentMessageRow, valent_message_row, GTK_TYPE_LIST_BOX_ROW)

typedef enum {
  PROP_CONTACT = 1,
  PROP_MESSAGE,
} ValentMessageRowProperty;

static GParamSpec *properties[PROP_MESSAGE + 1] = { NULL, };

static char *
_valent_message_get_excerpt (ValentMessageRow *self,
                             ValentMessage    *message)
{
  const char *body = NULL;

  if (message != NULL)
    body = valent_message_get_text (message);

  if (body != NULL && *body != '\0')
    {
      g_auto (GStrv) parts = g_strsplit (body, "\n", 2);

      if (valent_message_get_box (message) == VALENT_MESSAGE_BOX_SENT)
        return g_strdup_printf (_("You: %s"), parts[0]);

      return g_strdup (parts[0]);
    }

  return g_strdup (body);
}

static void
valent_message_row_sync (ValentMessageRow *row)
{
  g_assert (VALENT_IS_MESSAGE_ROW (row));

  if (row->message != NULL && !valent_message_get_read (row->message))
    {
      gtk_widget_add_css_class (row->title_label, "unread");
      gtk_widget_add_css_class (row->subtitle_label, "unread");
    }
  else
    {
      gtk_widget_remove_css_class (row->title_label, "unread");
      gtk_widget_remove_css_class (row->subtitle_label, "unread");
    }
}

/*
 * GObject
 */
static void
valent_message_row_finalize (GObject *object)
{
  ValentMessageRow *self = VALENT_MESSAGE_ROW (object);

  g_clear_object (&self->contact);
  g_clear_object (&self->message);

  G_OBJECT_CLASS (valent_message_row_parent_class)->finalize (object);
}

static void
valent_message_row_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ValentMessageRow *self = VALENT_MESSAGE_ROW (object);

  switch ((ValentMessageRowProperty)prop_id)
    {
    case PROP_CONTACT:
      g_value_set_object (value, self->contact);
      break;

    case PROP_MESSAGE:
      g_value_set_object (value, self->message);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_row_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ValentMessageRow *self = VALENT_MESSAGE_ROW (object);

  switch ((ValentMessageRowProperty)prop_id)
    {
    case PROP_CONTACT:
      valent_message_row_set_contact (self, g_value_get_object (value));
      break;

    case PROP_MESSAGE:
      valent_message_row_set_message (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_row_class_init (ValentMessageRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_message_row_finalize;
  object_class->get_property = valent_message_row_get_property;
  object_class->set_property = valent_message_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-message-row.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentMessageRow, avatar);
  gtk_widget_class_bind_template_child (widget_class, ValentMessageRow, title_label);
  gtk_widget_class_bind_template_child (widget_class, ValentMessageRow, subtitle_label);
  gtk_widget_class_bind_template_child (widget_class, ValentMessageRow, date_label);
  gtk_widget_class_bind_template_callback (widget_class, _valent_message_get_excerpt);
  gtk_widget_class_bind_template_callback (widget_class, valent_contact_to_paintable);

  /**
   * ValentMessageRow:contact:
   *
   * The `EContact` that sent this message.
   */
  properties [PROP_CONTACT] =
    g_param_spec_object ("contact", NULL, NULL,
                         E_TYPE_CONTACT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessageRow:message:
   *
   * The message this row displays.
   */
  properties [PROP_MESSAGE] =
    g_param_spec_object ("message", NULL, NULL,
                         VALENT_TYPE_MESSAGE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  g_type_ensure (VALENT_TYPE_DATE_LABEL);
}

static void
valent_message_row_init (ValentMessageRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * valent_message_row_new:
 * @message: (nullable): a `ValentMessage`
 * @contact: (nullable): a `EContact`
 *
 * Create a new message row for @contact and @message.
 *
 * Returns: a `ValentMessageRow`
 */
GtkWidget *
valent_message_row_new (ValentMessage *message,
                        EContact      *contact)
{
  return g_object_new (VALENT_TYPE_MESSAGE_ROW,
                       "contact", contact,
                       "message", message,
                       NULL);
}

/**
 * valent_message_row_get_contact:
 * @row: a `ValentMessageRow`
 *
 * Get the contact.
 *
 * Returns: (transfer none) (nullable): a `ValentContact`
 */
EContact *
valent_message_row_get_contact (ValentMessageRow *row)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_ROW (row), NULL);

  return row->contact;
}

/**
 * valent_message_row_set_contact:
 * @row: a `ValentMessageRow`
 * @contact: a `ValentContact`
 *
 * Set or update the contact.
 */
void
valent_message_row_set_contact (ValentMessageRow *row,
                                EContact         *contact)
{
  g_return_if_fail (VALENT_IS_MESSAGE_ROW (row));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (g_set_object (&row->contact, contact))
    {
      valent_message_row_sync (row);
      g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
    }
}

/**
 * valent_message_row_get_message:
 * @row: a `ValentMessageRow`
 *
 * Get the message.
 *
 * Returns: (transfer none): a `ValentMessage`
 */
ValentMessage *
valent_message_row_get_message (ValentMessageRow *row)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_ROW (row), NULL);

  return row->message;
}

/**
 * valent_message_row_set_message:
 * @row: a `ValentMessageRow`
 * @message: a `ValentMessage`
 *
 * Set or update the message.
 */
void
valent_message_row_set_message (ValentMessageRow *row,
                                ValentMessage    *message)
{
  g_return_if_fail (VALENT_IS_MESSAGE_ROW (row));
  g_return_if_fail (message == NULL || VALENT_IS_MESSAGE (message));

  if (row->message == message)
    return;

  if (row->message != NULL)
    {
      g_signal_handlers_disconnect_by_func (row->message,
                                            valent_message_row_sync,
                                            row);
      g_clear_object (&row->message);
    }

  if (message != NULL)
    {
      row->message = g_object_ref (message);
      g_signal_connect_object (row->message,
                               "notify::read",
                               G_CALLBACK (valent_message_row_sync),
                               row,
                               G_CONNECT_SWAPPED);
    }

  valent_message_row_sync (row);
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_MESSAGE]);
}

