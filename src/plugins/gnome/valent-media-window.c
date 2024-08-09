// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media-window"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-media-remote.h"
#include "valent-ui-utils-private.h"

#include "valent-media-window.h"

struct _ValentMediaWindow
{
  AdwWindow   parent_instance;

  GListModel *players;
};

G_DEFINE_FINAL_TYPE (ValentMediaWindow, valent_media_window, ADW_TYPE_WINDOW)

typedef enum {
  PROP_PLAYERS = 1,
} ValentMediaWindowProperty;

static GParamSpec *properties[PROP_PLAYERS + 1] = { NULL, };

/*
 * GObject
 */
static void
valent_media_window_finalize (GObject *object)
{
  ValentMediaWindow *self = VALENT_MEDIA_WINDOW (object);

  g_clear_object (&self->players);

  G_OBJECT_CLASS (valent_media_window_parent_class)->finalize (object);
}

static void
valent_media_window_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentMediaWindow *self = VALENT_MEDIA_WINDOW (object);

  switch ((ValentMediaWindowProperty)prop_id)
    {
    case PROP_PLAYERS:
      g_value_set_object (value, self->players);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_media_window_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentMediaWindow *self = VALENT_MEDIA_WINDOW (object);

  switch ((ValentMediaWindowProperty)prop_id)
    {
    case PROP_PLAYERS:
      self->players = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_media_window_class_init (ValentMediaWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = valent_media_window_finalize;
  object_class->get_property = valent_media_window_get_property;
  object_class->set_property = valent_media_window_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-media-window.ui");

  properties [PROP_PLAYERS] =
    g_param_spec_object ("players", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_media_window_init (ValentMediaWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
