// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media-window"

#include "config.h"

#include <adwaita.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <valent.h>

#include "valent-media-remote.h"
#include "valent-mixer-remote.h"
#include "valent-ui-utils-private.h"

#include "valent-media-window.h"

struct _ValentMediaWindow
{
  AdwWindow    parent_instance;

  GListModel  *mixers;
  GListModel  *players;

  /* template */
  GtkDropDown *media_player;
  GtkStack    *media_stack;
  GtkDropDown *mixer_adapter;
};

G_DEFINE_FINAL_TYPE (ValentMediaWindow, valent_media_window, ADW_TYPE_WINDOW)

typedef enum {
  PROP_MIXERS = 1,
  PROP_PLAYERS,
} ValentMediaWindowProperty;

static char *
_valent_mixer_adapter_get_name (GtkListItem *item)
{
  g_autoptr (PeasPluginInfo) plugin_info = NULL;
  ValentExtension *extension = NULL;
  ValentObject *parent = NULL;

  extension = gtk_list_item_get_item (item);
  if (extension == NULL)
    return NULL;

  g_object_get (extension, "plugin-info", &plugin_info, NULL);
  if (plugin_info != NULL)
    return g_strdup (peas_plugin_info_get_name (plugin_info));

  parent = valent_object_get_parent (VALENT_OBJECT (extension));
  if (VALENT_IS_DEVICE (parent))
    return g_strdup (valent_device_get_name (VALENT_DEVICE (parent)));

  return g_strdup (valent_object_get_iri (VALENT_OBJECT (extension)));
}

static GParamSpec *properties[PROP_PLAYERS + 1] = { NULL, };

static void
on_player_selected (GtkDropDown       *dropdown,
                    GParamSpec        *pspec,
                    ValentMediaWindow *self)
{
  ValentMediaPlayer *player = NULL;
  ValentObject *player_parent = NULL;
  unsigned int n_items = 0;

  g_assert (VALENT_IS_MEDIA_WINDOW (self));

  player = gtk_drop_down_get_selected_item (self->media_player);
  if (player == NULL)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->media_player), FALSE);
      gtk_stack_set_visible_child_name (self->media_stack, "empty-state");
      return;
    }

  player_parent = valent_object_get_parent (VALENT_OBJECT (player));
  if (player_parent == NULL)
    return;

  n_items = g_list_model_get_n_items (self->mixers);
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (ValentMixerAdapter) item = NULL;
      ValentObject *item_parent = NULL;

      item = g_list_model_get_item (self->mixers, i);
      item_parent = valent_object_get_parent (VALENT_OBJECT (item));
      if (item_parent == player_parent)
        {
          gtk_drop_down_set_selected (self->mixer_adapter, i);
          break;
        }

      // TODO: this should only be reached for local players, whose direct
      //       source doesn't match the player. The hypothetical solution is
      //       `valent_object_get_ancestor (object, VALENT_TYPE_DATA_SOURCE)`
      if (!VALENT_IS_DEVICE (player_parent) && !VALENT_IS_DEVICE (item_parent))
        {
          gtk_drop_down_set_selected (self->mixer_adapter, i);
          break;
        }
    }

  gtk_widget_set_visible (GTK_WIDGET (self->media_player), TRUE);
  gtk_stack_set_visible_child_name (self->media_stack, "player");
}

/*
 * GObject
 */
static void
valent_media_window_finalize (GObject *object)
{
  ValentMediaWindow *self = VALENT_MEDIA_WINDOW (object);

  g_clear_object (&self->mixers);
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
    case PROP_MIXERS:
      g_value_set_object (value, self->mixers);
      break;

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
    case PROP_MIXERS:
      self->mixers = g_value_dup_object (value);
      break;

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

  properties [PROP_MIXERS] =
    g_param_spec_object ("mixers", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  properties [PROP_PLAYERS] =
    g_param_spec_object ("players", NULL, NULL,
                         G_TYPE_LIST_MODEL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/plugins/gnome/valent-media-window.ui");
  gtk_widget_class_bind_template_child (widget_class, ValentMediaWindow, media_player);
  gtk_widget_class_bind_template_child (widget_class, ValentMediaWindow, media_stack);
  gtk_widget_class_bind_template_child (widget_class, ValentMediaWindow, mixer_adapter);
  gtk_widget_class_bind_template_callback (widget_class, on_player_selected);
  gtk_widget_class_bind_template_callback (widget_class, _valent_mixer_adapter_get_name);
}

static void
valent_media_window_init (ValentMediaWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
