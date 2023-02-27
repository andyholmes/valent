// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-presenter-remote"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-presenter-remote.h"


struct _ValentPresenterRemote
{
  AdwWindow     parent_instance;

  ValentDevice *device;
};

G_DEFINE_FINAL_TYPE (ValentPresenterRemote, valent_presenter_remote, ADW_TYPE_WINDOW)

enum {
  PROP_0,
  PROP_DEVICE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/*
 * File Filter
 */
static const char *mimetypes[] = {
  "application/vnd.ms-powerpoint",
  "application/vnd.ms-powerpoint.presentation.macroEnabled.12",
  "application/vnd.ms-powerpoint.slide.macroEnabled.12",
  "application/vnd.ms-powerpoint.slideshow.macroEnabled.12",
  "application/vnd.oasis.opendocument.presentation",
  "application/vnd.oasis.opendocument.presentation-flat-xml",
  "application/vnd.openxmlformats-officedocument.presentationml.presentation",
  "application/vnd.openxmlformats-officedocument.presentationml.slide",
  "application/vnd.openxmlformats-officedocument.presentationml.slideshow",
  NULL
};

/*
 * GtkWidget
 */
static void
presenter_open_response (GtkNativeDialog *dialog,
                         int              response_id,
                         gpointer         user_data)
{
  ValentPresenterRemote *self = VALENT_PRESENTER_REMOTE (user_data);

  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      g_autoptr (GFile) file = NULL;
      g_autofree char *uri = NULL;

      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      uri = g_file_get_uri (file);

      g_action_group_activate_action (G_ACTION_GROUP (self->device),
                                      "share.open",
                                      g_variant_new_string (uri));
    }

  gtk_native_dialog_destroy (dialog);
}

static void
presenter_open_action (GtkWidget  *widget,
                       const char *action_name,
                       GVariant   *parameter)
{
  ValentPresenterRemote *self = VALENT_PRESENTER_REMOTE (widget);
  GtkNativeDialog *dialog;
  g_autoptr (GtkFileFilter) presentations_filter = NULL;
  g_autoptr (GtkFileFilter) all_filter = NULL;

  g_assert (VALENT_IS_PRESENTER_REMOTE (self));

  /* Select single files */
  dialog = g_object_new (GTK_TYPE_FILE_CHOOSER_NATIVE,
                         "title",           _("Select Presentation"),
                         "accept-label",    _("Open"),
                         "cancel-label",    _("Cancel"),
                         "action",          GTK_FILE_CHOOSER_ACTION_OPEN,
                         "select-multiple", FALSE,
                         "modal",           TRUE,
                         "transient-for",   self,
                         NULL);

  /* Filter presentation files */
  presentations_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (presentations_filter, _("Presentations"));

  for (unsigned int i = 0; mimetypes[i] != NULL; i++)
    gtk_file_filter_add_mime_type (presentations_filter, mimetypes[i]);

  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), presentations_filter);

  all_filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (all_filter, _("All Files"));
  gtk_file_filter_add_pattern (all_filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), all_filter);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (presenter_open_response),
                    self);

  gtk_native_dialog_show (dialog);
}

/*
 * GObject
 */
static void
valent_presenter_remote_constructed (GObject *object)
{
  ValentPresenterRemote *self = VALENT_PRESENTER_REMOTE (object);

  gtk_widget_insert_action_group (GTK_WIDGET (self),
                                  "device",
                                  G_ACTION_GROUP (self->device));

  G_OBJECT_CLASS (valent_presenter_remote_parent_class)->constructed (object);
}

static void
valent_presenter_remote_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ValentPresenterRemote *self = VALENT_PRESENTER_REMOTE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, self->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_presenter_remote_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ValentPresenterRemote *self = VALENT_PRESENTER_REMOTE (object);

  switch (prop_id)
    {
    case PROP_DEVICE:
      self->device = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_presenter_remote_class_init (ValentPresenterRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = valent_presenter_remote_constructed;
  object_class->get_property = valent_presenter_remote_get_property;
  object_class->set_property = valent_presenter_remote_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/presenter/valent-presenter-remote.ui");

  gtk_widget_class_install_action (widget_class, "remote.open", NULL, presenter_open_action);

  properties [PROP_DEVICE] =
    g_param_spec_object ("device", NULL, NULL,
                         VALENT_TYPE_DEVICE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_presenter_remote_init (ValentPresenterRemote *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

