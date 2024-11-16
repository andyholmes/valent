// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mixer-remote"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <pango/pango.h>
#include <valent.h>

#include "valent-mixer-remote.h"
#include "valent-ui-utils-private.h"

struct _ValentMixerRemote
{
  AdwBreakpointBin    parent_instance;

  ValentMixerAdapter *adapter;

  /* template */
  GtkListBox         *stream_list;
  GListModel         *model;
  GtkFilter          *filter;
  GtkStringSorter    *sorter;
  GtkWidget          *group;
};

G_DEFINE_FINAL_TYPE (ValentMixerRemote, valent_mixer_remote, ADW_TYPE_BREAKPOINT_BIN)

typedef enum {
  PROP_MIXER = 1,
} ValentMixerRemoteProperty;

static GParamSpec *properties[PROP_MIXER + 1] = { NULL, };

static gboolean
valent_mixer_output_filter (gpointer item,
                            gpointer user_data)
{
  ValentMixerStream *stream = VALENT_MIXER_STREAM (item);

  if (valent_mixer_stream_get_direction (stream) == VALENT_MIXER_OUTPUT)
    return TRUE;

  return FALSE;
}

static gboolean
on_change_value (GtkRange          *range,
                 GtkScrollType      scroll,
                 double             value,
                 ValentMixerStream *stream)
{
  GtkAdjustment *adjustment;
  double lower, upper, page_size;

  g_assert (VALENT_IS_MIXER_STREAM (stream));

  adjustment = gtk_range_get_adjustment (range);
  lower = gtk_adjustment_get_lower (adjustment);
  upper = gtk_adjustment_get_upper (adjustment);
  page_size = gtk_adjustment_get_page_size (adjustment);
  value = CLAMP (value, lower, (upper - page_size));

  gtk_adjustment_set_value (adjustment, value);
  valent_mixer_stream_set_level (stream, (unsigned int)round (value));

  return GDK_EVENT_STOP;
}

static void
update_icon_cb (ValentMixerStream *stream,
                GParamSpec        *pspec,
                GtkButton         *button)
{
  unsigned int level = 0;

  if (valent_mixer_stream_get_muted (stream))
    {
      gtk_button_set_icon_name (button, "audio-volume-muted-symbolic");
      return;
    }

  level = valent_mixer_stream_get_level (stream);
  if (level >= 70)
    gtk_button_set_icon_name (button, "audio-volume-high-symbolic");
  else if (level >= 30)
    gtk_button_set_icon_name (button, "audio-volume-medium-symbolic");
  else if (level > 0)
    gtk_button_set_icon_name (button, "audio-volume-low-symbolic");
  else
    gtk_button_set_icon_name (button, "audio-volume-muted-symbolic");
}

static void
on_stream_selected (GtkCheckButton    *button,
                    GParamSpec        *pspec,
                    ValentMixerRemote *self)
{
  if (gtk_check_button_get_active (button))
    {
      ValentMixerStream *stream = NULL;

      stream = g_object_get_data (G_OBJECT (button), "valent-mixer-stream");
      valent_mixer_adapter_set_default_output (self->adapter, stream);
    }
}

/*
 * ValentMixerRemote
 */
static GtkWidget *
stream_list_create (gpointer item,
                    gpointer user_data)
{
  ValentMixerRemote *self = VALENT_MIXER_REMOTE (user_data);
  ValentMixerStream *stream = VALENT_MIXER_STREAM (item);
  ValentMixerStream *default_output = NULL;
  GtkAdjustment *adjustment;
  GtkWidget *row;
  GtkWidget *grid;
  GtkWidget *radio;
  GtkWidget *button;
  GtkWidget *label;
  GtkWidget *slider;

  default_output = valent_mixer_adapter_get_default_output (self->adapter);

  grid = g_object_new (GTK_TYPE_GRID,
                       "row-spacing", 8,
                       NULL);

  radio = g_object_new (GTK_TYPE_CHECK_BUTTON,
                        "active", (default_output == stream),
                        "group",  self->group,
                        "valign", GTK_ALIGN_CENTER,
                        NULL);
  g_signal_connect_object (radio,
                           "toggled",
                           G_CALLBACK (on_stream_selected),
                           self,
                           G_CONNECT_DEFAULT);
  g_object_set_data (G_OBJECT (radio), "valent-mixer-stream", stream);
  gtk_grid_attach (GTK_GRID (grid), radio, 0, 0, 2, 1);

  if (self->group == NULL)
    self->group = radio;

  label = g_object_new (GTK_TYPE_LABEL,
                        "label",     valent_mixer_stream_get_description (stream),
                        "ellipsize", PANGO_ELLIPSIZE_END,
                        "xalign",    0.0,
                        NULL);
  g_object_bind_property (stream, "description",
                          label,  "label",
                          G_BINDING_DEFAULT);
  gtk_check_button_set_child (GTK_CHECK_BUTTON (radio), label);

  button = g_object_new (GTK_TYPE_TOGGLE_BUTTON,
                         "active",    valent_mixer_stream_get_muted (stream),
                         "icon-name", "audio-volume-medium-symbolic",
                         NULL);
  gtk_widget_add_css_class (button, "circular");
  gtk_widget_add_css_class (button, "flat");
  g_signal_connect_object (stream,
                           "notify::muted",
                           G_CALLBACK (update_icon_cb),
                           button,
                           G_CONNECT_DEFAULT);
  g_object_bind_property (stream, "muted",
                          button, "active",
                          G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
  gtk_grid_attach (GTK_GRID (grid), button, 0, 1, 1, 1);

  adjustment = gtk_adjustment_new (valent_mixer_stream_get_level (stream),
                                   0.0, 110.0,
                                   1.0,   // step increment
                                   2.0,   // page increment
                                   10.0); // page size
  g_object_bind_property (stream,     "level",
                          adjustment, "value",
                          G_BINDING_DEFAULT);
  g_signal_connect_object (stream,
                           "notify::level",
                           G_CALLBACK (update_icon_cb),
                           button,
                           G_CONNECT_DEFAULT);
  slider = g_object_new (GTK_TYPE_SCALE,
                        "adjustment", adjustment,
                        "hexpand",    TRUE,
                        NULL);
  g_signal_connect_object (slider,
                           "change-value",
                           G_CALLBACK (on_change_value),
                           stream,
                           G_CONNECT_DEFAULT);
  gtk_grid_attach (GTK_GRID (grid), slider, 1, 1, 1, 1);

  row = g_object_new (GTK_TYPE_LIST_BOX_ROW,
                      "activatable", FALSE,
                      "selectable",  FALSE,
                      "child",       grid,
                      NULL);
  gtk_widget_add_css_class (row, "valent-mixer-row");
  gtk_accessible_update_relation (GTK_ACCESSIBLE (row),
                                  GTK_ACCESSIBLE_RELATION_DESCRIBED_BY, label, NULL,
                                  -1);

  return row;
}

static void
valent_mixer_remote_set_mixer (ValentMixerRemote  *self,
                               ValentMixerAdapter *mixer)
{
  g_assert (VALENT_IS_MIXER_REMOTE (self));
  g_assert (mixer == NULL || VALENT_IS_MIXER_ADAPTER (mixer));

  if (g_set_object (&self->adapter, mixer))
    {
      self->group = NULL;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MIXER]);
    }
}

/*
 * GObject
 */
static void
valent_mixer_remote_dispose (GObject *object)
{
  ValentMixerRemote *self = VALENT_MIXER_REMOTE (object);

  if (self->adapter != NULL)
    {
      g_signal_handlers_disconnect_by_data (self->adapter, self);
      g_clear_object (&self->adapter);
    }

  G_OBJECT_CLASS (valent_mixer_remote_parent_class)->dispose (object);
}

static void
valent_mixer_remote_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ValentMixerRemote *self = VALENT_MIXER_REMOTE (object);

  switch ((ValentMixerRemoteProperty)prop_id)
    {
    case PROP_MIXER:
      g_value_set_object (value, self->adapter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_remote_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ValentMixerRemote *self = VALENT_MIXER_REMOTE (object);

  switch ((ValentMixerRemoteProperty)prop_id)
    {
    case PROP_MIXER:
      valent_mixer_remote_set_mixer (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_mixer_remote_class_init (ValentMixerRemoteClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = valent_mixer_remote_dispose;
  object_class->get_property = valent_mixer_remote_get_property;
  object_class->set_property = valent_mixer_remote_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-mixer-remote.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentMixerRemote, stream_list);
  gtk_widget_class_bind_template_child (widget_class, ValentMixerRemote, filter);
  gtk_widget_class_bind_template_child (widget_class, ValentMixerRemote, model);

  properties [PROP_MIXER] =
    g_param_spec_object ("mixer", NULL, NULL,
                         VALENT_TYPE_MIXER_ADAPTER,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_mixer_remote_init (ValentMixerRemote *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_custom_filter_set_filter_func (GTK_CUSTOM_FILTER (self->filter),
                                     valent_mixer_output_filter,
                                     self,
                                     NULL);
  gtk_list_box_bind_model (self->stream_list,
                           G_LIST_MODEL (self->model),
                           stream_list_create,
                           self,
                           NULL);
}

