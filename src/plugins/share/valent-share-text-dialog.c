// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-share-text-dialog"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-share-text-dialog.h"


struct _ValentShareTextDialog
{
  AdwMessageDialog  parent_instance;

  char             *text;
  GtkLabel         *text_label;
};

G_DEFINE_FINAL_TYPE (ValentShareTextDialog, valent_share_text_dialog, ADW_TYPE_MESSAGE_DIALOG)

enum {
  PROP_0,
  PROP_TEXT,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static void
g_file_replace_contents_cb (GFile        *file,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  g_autoptr (GError) error = NULL;

  if (!g_file_replace_contents_finish (file, result, NULL, &error))
    g_warning ("\"%s\": %s", g_file_peek_path (file), error->message);
}

static void
gtk_file_dialog_save_cb (GtkFileDialog         *dialog,
                         GAsyncResult          *result,
                         ValentShareTextDialog *self)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  if ((file = gtk_file_dialog_save_finish (dialog, result, &error)) == NULL)
    {
      if (!g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED))
        g_warning ("%s(): %s", G_STRFUNC, error->message);

      return;
    }

  bytes = g_bytes_new (self->text, strlen (self->text));
  g_file_replace_contents_bytes_async (file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_REPLACE_DESTINATION,
                                       NULL,
                                       (GAsyncReadyCallback)g_file_replace_contents_cb,
                                       NULL);
  gtk_window_destroy (GTK_WINDOW (self));
}

/*
 * AdwMessageDialog
 */
static void
valent_share_text_dialog_response (AdwMessageDialog *dialog,
                                   const char       *response)
{
  ValentShareTextDialog *self = VALENT_SHARE_TEXT_DIALOG (dialog);

  g_assert (VALENT_IS_SHARE_TEXT_DIALOG (self));

  if (g_strcmp0 (response, "copy") == 0)
    {
      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (dialog)),
                              self->text);
      gtk_window_destroy (GTK_WINDOW (dialog));
    }
  else if (g_strcmp0 (response, "save") == 0)
    {
      g_autoptr (GtkFileDialog) chooser = NULL;

      chooser = g_object_new (GTK_TYPE_FILE_DIALOG,
                              "accept-label", _("Save"),
                              "modal",        TRUE,
                              NULL);
      gtk_file_dialog_save (chooser,
                            GTK_WINDOW (dialog),
                            NULL,
                            (GAsyncReadyCallback)gtk_file_dialog_save_cb,
                            self);
    }
  else if (g_strcmp0 (response, "close") == 0)
    {
      gtk_window_destroy (GTK_WINDOW (dialog));
    }
}

/*
 * GtkWindow
 */
static gboolean
valent_share_text_dialog_close_request (GtkWindow *window)
{
  /* Chain-up to AdwMessageDialog to avoid re-entrancy with `response` */
  GTK_WINDOW_CLASS (valent_share_text_dialog_parent_class)->close_request (window);

  /* Unconditionally block `close-request` */
  return TRUE;
}

/*
 * GObject
 */
static void
valent_share_text_dialog_dispose (GObject *object)
{
  GtkWidget *widget = GTK_WIDGET (object);

  gtk_widget_dispose_template (widget, VALENT_TYPE_SHARE_TEXT_DIALOG);

  G_OBJECT_CLASS (valent_share_text_dialog_parent_class)->dispose (object);
}

static void
valent_share_text_dialog_finalize (GObject *object)
{
  ValentShareTextDialog *self = VALENT_SHARE_TEXT_DIALOG (object);

  g_clear_pointer (&self->text, g_free);

  G_OBJECT_CLASS (valent_share_text_dialog_parent_class)->finalize (object);
}

static void
valent_share_text_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ValentShareTextDialog *self = VALENT_SHARE_TEXT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, valent_share_text_dialog_get_text (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_text_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ValentShareTextDialog *self = VALENT_SHARE_TEXT_DIALOG (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      valent_share_text_dialog_set_text (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_share_text_dialog_class_init (ValentShareTextDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkWindowClass *window_class = GTK_WINDOW_CLASS (klass);
  AdwMessageDialogClass *dialog_class = ADW_MESSAGE_DIALOG_CLASS (klass);

  object_class->finalize = valent_share_text_dialog_finalize;
  object_class->dispose = valent_share_text_dialog_dispose;
  object_class->get_property = valent_share_text_dialog_get_property;
  object_class->set_property = valent_share_text_dialog_set_property;

  window_class->close_request = valent_share_text_dialog_close_request;

  dialog_class->response = valent_share_text_dialog_response;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/share/valent-share-text-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentShareTextDialog, text_label);

  /**
   * ValentShareTextDialog:text:
   *
   * The text content shared from the remote [class@Valent.Device].
   */
  properties [PROP_TEXT] =
    g_param_spec_string ("text", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_share_text_dialog_init (ValentShareTextDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

/**
 * valent_share_text_dialog_get_text:
 * @dialog: a `ValentShareTextDialog`
 *
 * Get the text content shared by the remote [class@Valent.Device].
 *
 * Returns: (transfer none) (nullable): the text content
 */
const char *
valent_share_text_dialog_get_text (ValentShareTextDialog *dialog)
{
  g_assert (VALENT_IS_SHARE_TEXT_DIALOG (dialog));

  return dialog->text;
}

/**
 * valent_share_text_dialog_set_text:
 * @self: a `ValentShareTextDialog`
 * @text: (nullable): text content
 *
 * Set the text content shared by the remote [class@Valent.Device].
 */
void
valent_share_text_dialog_set_text (ValentShareTextDialog *dialog,
                                   const char            *text)
{
  g_assert (VALENT_IS_SHARE_TEXT_DIALOG (dialog));

  if (g_set_str (&dialog->text, text))
    {
      g_autofree char *markup = NULL;

      markup = valent_string_to_markup (dialog->text);
      gtk_label_set_markup (dialog->text_label, markup);
      g_object_notify_by_pspec (G_OBJECT (dialog), properties [PROP_TEXT]);
    }
}

