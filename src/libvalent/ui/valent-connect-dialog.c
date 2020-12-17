// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <glib/gi18n.h>

#include "valent-connect-dialog.h"


struct _ValentConnectDialog
{
  GtkDialog  parent_instance;

  /* Template widgets */
  GtkButton *cancel_button;
  GtkButton *connect_button;
  GtkGrid   *layout;
};

G_DEFINE_TYPE (ValentConnectDialog, valent_connect_dialog, GTK_TYPE_DIALOG)


/*
 * GtkDialog
 */
static void
valent_connect_dialog_response (GtkDialog *dialog,
                                int        response_id)
{
  //ValentConnectDialog *self = VALENT_CONNECT_DIALOG (dialog);

  if (response_id == GTK_RESPONSE_OK)
    {
      GtkApplication *application;

      application = gtk_window_get_application (GTK_WINDOW (dialog));

      if (application != NULL)
        g_action_group_activate_action (G_ACTION_GROUP (application),
                                        "identify",
                                        NULL);
    }

  gtk_window_destroy (GTK_WINDOW (dialog));
}


/*
 * GObject
 */
static void
valent_connect_dialog_class_init (ValentConnectDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/ca/andyholmes/Valent/ui/valent-connect-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentConnectDialog, cancel_button);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectDialog, connect_button);
  gtk_widget_class_bind_template_child (widget_class, ValentConnectDialog, layout);

  dialog_class->response = valent_connect_dialog_response;
}

static void
valent_connect_dialog_init (ValentConnectDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkDialog *
valent_connect_dialog_new (void)
{
  return g_object_new (VALENT_TYPE_CONNECT_DIALOG,
                       "use-header-bar", TRUE,
                       NULL);
}

