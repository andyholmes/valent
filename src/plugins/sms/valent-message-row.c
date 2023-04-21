// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-message-row"

#include "config.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

#include "valent-date-label.h"
#include "valent-message-row.h"
#include "valent-message.h"
#include "valent-sms-utils.h"


struct _ValentMessageRow
{
  GtkListBoxRow  parent_instance;

  ValentMessage *message;
  EContact      *contact;

  GtkWidget     *grid;
  GtkWidget     *avatar;
  GtkWidget     *name_label;
  GtkWidget     *date_label;
  GtkWidget     *body_label;
};

G_DEFINE_FINAL_TYPE (ValentMessageRow, valent_message_row, GTK_TYPE_LIST_BOX_ROW)


enum {
  PROP_0,
  PROP_CONTACT,
  PROP_MESSAGE,
  PROP_THREAD_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


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

  switch (prop_id)
    {
    case PROP_CONTACT:
      g_value_set_object (value, self->contact);
      break;

    case PROP_MESSAGE:
      g_value_set_object (value, self->message);
      break;

    case PROP_THREAD_ID:
      g_value_set_int64 (value, valent_message_row_get_thread_id (self));
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

  switch (prop_id)
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

  object_class->finalize = valent_message_row_finalize;
  object_class->get_property = valent_message_row_get_property;
  object_class->set_property = valent_message_row_set_property;

  /**
   * ValentMessageRow:contact
   *
   * The #EContact that sent this message.
   */
  properties [PROP_CONTACT] =
    g_param_spec_object ("contact", NULL, NULL,
                         E_TYPE_CONTACT,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessageRow:message
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

  /**
   * ValentMessageRow:thread-id
   *
   * The thread id this message belongs to.
   */
  properties [PROP_THREAD_ID] =
    g_param_spec_int64 ("thread-id", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READABLE |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_message_row_init (ValentMessageRow *self)
{
  gtk_widget_add_css_class (GTK_WIDGET (self), "valent-message-row");

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
                                   "ellipsize",  PANGO_ELLIPSIZE_END,
                                   "halign",     GTK_ALIGN_START,
                                   "hexpand",    TRUE,
                                   "valign",     GTK_ALIGN_START,
                                   "vexpand",    TRUE,
                                   "use-markup", TRUE,
                                   "xalign",     0.0,
                                   NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->name_label, 1, 0, 1, 1);

  self->date_label = g_object_new (VALENT_TYPE_DATE_LABEL,
                                   "halign",     GTK_ALIGN_END,
                                   NULL);
  gtk_widget_add_css_class (self->date_label, "dim-label");
  gtk_grid_attach (GTK_GRID (self->grid), self->date_label, 2, 0, 1, 1);

  self->body_label = g_object_new (GTK_TYPE_LABEL,
                                   "ellipsize",        PANGO_ELLIPSIZE_END,
                                   "halign",           GTK_ALIGN_START,
                                   "hexpand",          TRUE,
                                   "valign",           GTK_ALIGN_END,
                                   "vexpand",          TRUE,
                                   "single-line-mode", TRUE,
                                   "use-markup",       TRUE,
                                   "xalign",           0.0,
                                   NULL);
  gtk_grid_attach (GTK_GRID (self->grid), self->body_label, 1, 1, 2, 1);
}

/**
 * valent_message_row_new:
 * @message: (nullable): a #ValentMessage
 * @contact: (nullable): a #EContact
 *
 * Create a new message row for @contact and @message.
 *
 * Returns: a #ValentMessageRow
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
 * @row: a #ValentMessageRow
 *
 * Get the contact.
 *
 * Returns: (transfer none) (nullable): a #ValentContact
 */
EContact *
valent_message_row_get_contact (ValentMessageRow *row)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_ROW (row), NULL);

  return row->contact;
}

/**
 * valent_message_row_set_contact:
 * @row: a #ValentMessageRow
 * @contact: a #ValentContact
 *
 * Set or update the contact.
 */
void
valent_message_row_set_contact (ValentMessageRow *row,
                                EContact         *contact)
{
  g_return_if_fail (VALENT_IS_MESSAGE_ROW (row));
  g_return_if_fail (contact == NULL || E_IS_CONTACT (contact));

  if (!g_set_object (&row->contact, contact))
    return;

  if (row->contact != NULL)
    valent_sms_avatar_from_contact (ADW_AVATAR (row->avatar), contact);

  valent_message_row_update (row);
  g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_CONTACT]);
}

/**
 * valent_message_row_get_thread_id:
 * @row: a #ValentMessageRow
 *
 * Get the thread_id of the message.
 *
 * Returns: a thread id
 */
int64_t
valent_message_row_get_thread_id (ValentMessageRow *row)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_ROW (row), 0);

  if G_UNLIKELY (row->message == NULL)
    return 0;

  return valent_message_get_thread_id (row->message);
}

/**
 * valent_message_row_get_message:
 * @row: a #ValentMessageRow
 *
 * Get the message.
 *
 * Returns: (transfer none): a #ValentMessage
 */
ValentMessage *
valent_message_row_get_message (ValentMessageRow *row)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_ROW (row), NULL);

  return row->message;
}

/**
 * valent_message_row_set_message:
 * @row: a #ValentMessageRow
 * @message: a #ValentMessage
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
      g_signal_handlers_disconnect_by_data (row->message, row);
      g_clear_object (&row->message);
    }

  if (message != NULL)
    {
      row->message = g_object_ref (message);
      g_signal_connect_swapped (row->message,
                                "notify",
                                G_CALLBACK (valent_message_row_update),
                                row);

      valent_message_row_update (row);
      g_object_notify_by_pspec (G_OBJECT (row), properties [PROP_MESSAGE]);
    }
}

/**
 * valent_message_row_update:
 * @row: a #ValentMessageRow
 *
 * Update the conversation row with data from #ValentMessageRow:contact
 * and #ValentMessageRow:message properties.
 */
void
valent_message_row_update (ValentMessageRow *row)
{
  gboolean read;
  const char *body;
  const char *name;
  int64_t date;
  g_autofree char *body_label = NULL;
  g_autofree char *name_label = NULL;

  g_return_if_fail (VALENT_IS_MESSAGE_ROW (row));

  if (row->message == NULL)
    return;

  /* Message Sender/Name */
  if (E_IS_CONTACT (row->contact))
    name = e_contact_get_const (row->contact, E_CONTACT_FULL_NAME);
  else
    name = valent_message_get_sender (row->message);

  /* Message Read/Unread */
  if ((read = valent_message_get_read (row->message)))
    name_label = g_strdup (name);
  else
    name_label = g_strdup_printf ("<b>%s</b>", name);

  gtk_label_set_label (GTK_LABEL (row->name_label), name_label);

  /* Message Body */
  if ((body = valent_message_get_text (row->message)) != NULL)
    {
      g_autofree char *text = NULL;

      text = g_markup_escape_text (body, -1);

      if (valent_message_get_box (row->message) == VALENT_MESSAGE_BOX_SENT)
        body_label = g_strdup_printf ("<small>You: %s</small>", text);
      else if (read)
        body_label = g_strdup_printf ("<small>%s</small>", text);
      else
        body_label = g_strdup_printf ("<b><small>%s</small></b>", text);
    }

  gtk_label_set_label (GTK_LABEL (row->body_label), body_label);

  /* Message Date */
  date = valent_message_get_date (row->message);
  valent_date_label_set_date (VALENT_DATE_LABEL (row->date_label), date);
}

