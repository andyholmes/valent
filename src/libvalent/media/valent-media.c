// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-media.h"
#include "valent-media-adapter.h"
#include "valent-media-player.h"


/**
 * ValentMedia:
 *
 * A class for monitoring and controlling media players.
 *
 * #ValentMedia is an aggregator of media players, intended for use by
 * [class@Valent.DevicePlugin] implementations.
 *
 * Plugins can implement [class@Valent.MediaAdapter] to provide an
 * interface to manage instances of [class@Valent.MediaPlayer].
 *
 * Since: 1.0
 */

struct _ValentMedia
{
  ValentComponent  parent_instance;

  GPtrArray       *adapters;
  GListModel      *exports;
  GPtrArray       *players;
  GPtrArray       *paused;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMedia, valent_media, VALENT_TYPE_COMPONENT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

static ValentMedia *default_media = NULL;


static void
on_items_changed (GListModel   *list,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  ValentMedia  *self)
{
  unsigned int real_position = 0;

  VALENT_ENTRY;

  g_assert (G_IS_LIST_MODEL (list));
  g_assert (VALENT_IS_MEDIA (self));

  /* Translate the adapter position */
  for (unsigned int i = 0; i < self->adapters->len; i++)
    {
      GListModel *adapter = g_ptr_array_index (self->adapters, i);

      if (adapter == list)
        break;

      real_position += g_list_model_get_n_items (adapter);
    }

  real_position += position;

  /* Propagate the changes */
  for (unsigned int i = 0; i < removed; i++)
    {
      g_autoptr (ValentMediaPlayer) player = NULL;

      player = g_ptr_array_steal_index (self->players, real_position);
      g_ptr_array_remove (self->paused, player);

      VALENT_NOTE ("removed %s (%s)",
                   G_OBJECT_TYPE_NAME (player),
                   valent_media_player_get_name (player));
    }

  for (unsigned int i = 0; i < added; i++)
    {
      ValentMediaPlayer *player = NULL;

      player = g_list_model_get_item (list, position + i);
      g_ptr_array_insert (self->players, real_position + i, player);

      VALENT_NOTE ("added %s (%s)",
                   G_OBJECT_TYPE_NAME (player),
                   valent_media_player_get_name (player));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), real_position, removed, added);

  VALENT_EXIT;
}

/*
 * GListModel
 */
static gpointer
valent_media_get_item (GListModel   *list,
                       unsigned int  position)
{
  ValentMedia *self = VALENT_MEDIA (list);

  g_assert (VALENT_IS_MEDIA (self));

  if G_UNLIKELY (position >= self->players->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->players, position));
}

static GType
valent_media_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MEDIA_PLAYER;
}

static unsigned int
valent_media_get_n_items (GListModel *list)
{
  ValentMedia *self = VALENT_MEDIA (list);

  g_assert (VALENT_IS_MEDIA (self));

  return self->players->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_media_get_item;
  iface->get_item_type = valent_media_get_item_type;
  iface->get_n_items = valent_media_get_n_items;
}

/*
 * ValentComponent
 */
static void
valent_media_bind_extension (ValentComponent *component,
                             GObject         *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  GListModel *list = G_LIST_MODEL (extension);
  unsigned int n_exports = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));
  g_assert (VALENT_IS_MEDIA_ADAPTER (extension));

  g_ptr_array_add (self->adapters, g_object_ref (extension));
  on_items_changed (list, 0, 0, g_list_model_get_n_items (list), self);
  g_signal_connect_object (list,
                           "items-changed",
                           G_CALLBACK (on_items_changed),
                           self,
                           0);

  n_exports = g_list_model_get_n_items (G_LIST_MODEL (self->exports));

  for (unsigned int i = 0; i < n_exports; i++)
    {
      g_autoptr (ValentMediaPlayer) player = NULL;

      player = g_list_model_get_item (G_LIST_MODEL (self->exports), i);
      valent_media_adapter_export_player (VALENT_MEDIA_ADAPTER (extension),
                                          player);
    }

  VALENT_EXIT;
}

static void
valent_media_unbind_extension (ValentComponent *component,
                               GObject         *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  GListModel *list = G_LIST_MODEL (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));
  g_assert (VALENT_IS_MEDIA_ADAPTER (extension));

  if (!g_ptr_array_find (self->adapters, extension, NULL))
    {
      g_warning ("No such adapter \"%s\" found in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_signal_handlers_disconnect_by_func (list, on_items_changed, self);
  on_items_changed (list, 0, g_list_model_get_n_items (list), 0, self);
  g_ptr_array_remove (self->adapters, extension);

  VALENT_EXIT;
}

/*
 * ValentObject
 */
static void
valent_media_destroy (ValentObject *object)
{
  ValentMedia *self = VALENT_MEDIA (object);

  g_list_store_remove_all (G_LIST_STORE (self->exports));

  VALENT_OBJECT_CLASS (valent_media_parent_class)->destroy (object);
}

/*
 * GObject
 */
static void
valent_media_finalize (GObject *object)
{
  ValentMedia *self = VALENT_MEDIA (object);

  g_clear_object (&self->exports);
  g_clear_pointer (&self->adapters, g_ptr_array_unref);
  g_clear_pointer (&self->players, g_ptr_array_unref);
  g_clear_pointer (&self->paused, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_media_parent_class)->finalize (object);
}

static void
valent_media_class_init (ValentMediaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentObjectClass *vobject_class = VALENT_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_media_finalize;

  vobject_class->destroy = valent_media_destroy;

  component_class->bind_extension = valent_media_bind_extension;
  component_class->unbind_extension = valent_media_unbind_extension;
}

static void
valent_media_init (ValentMedia *self)
{
  self->adapters = g_ptr_array_new_with_free_func (g_object_unref);
  self->exports = G_LIST_MODEL (g_list_store_new (VALENT_TYPE_MEDIA_PLAYER));
  self->players = g_ptr_array_new_with_free_func (g_object_unref);
  self->paused = g_ptr_array_new ();

  g_ptr_array_add (self->adapters, g_object_ref (self->exports));
  g_signal_connect_object (self->exports,
                           "items-changed",
                           G_CALLBACK (on_items_changed),
                           self, 0);
}

/**
 * valent_media_get_default:
 *
 * Get the default [class@Valent.Media].
 *
 * Returns: (transfer none) (not nullable): a #ValentMedia
 *
 * Since: 1.0
 */
ValentMedia *
valent_media_get_default (void)
{
  if (default_media == NULL)
    {
      default_media = g_object_new (VALENT_TYPE_MEDIA,
                                    "plugin-domain", "media",
                                    "plugin-type",   VALENT_TYPE_MEDIA_ADAPTER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_media),
                                 (gpointer)&default_media);
    }

  return default_media;
}

/**
 * valent_media_export_player:
 * @media: a #ValentMedia
 * @player: a #ValentMediaPlayer
 *
 * Export @player on all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_media_export_player (ValentMedia       *media,
                            ValentMediaPlayer *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA (media));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  if (g_ptr_array_find (media->players, player, NULL))
    {
      g_critical ("%s(): known player %s (%s)",
                  G_STRFUNC,
                  G_OBJECT_TYPE_NAME (player),
                  valent_media_player_get_name (player));
      VALENT_EXIT;
    }

  // Starting at index `1` skips the exports GListModel
  for (unsigned int i = 1; i < media->adapters->len; i++)
    {
      ValentMediaAdapter *adapter = NULL;

      adapter = g_ptr_array_index (media->adapters, i);
      valent_media_adapter_export_player (adapter, player);
    }

  g_list_store_append (G_LIST_STORE (media->exports), player);

  VALENT_EXIT;
}

/**
 * valent_media_unexport_player:
 * @media: a #ValentMedia
 * @player: a #ValentMediaPlayer
 *
 * Unexport @player from all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_media_unexport_player (ValentMedia       *media,
                              ValentMediaPlayer *player)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA (media));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  if (!g_list_store_find (G_LIST_STORE (media->exports), player, &position))
    {
      g_critical ("%s(): unknown player %s (%s)",
                  G_STRFUNC,
                  G_OBJECT_TYPE_NAME (player),
                  valent_media_player_get_name (player));
      VALENT_EXIT;
    }

  // Starting at index `1` skips the exports GListModel
  for (unsigned int i = 1; i < media->adapters->len; i++)
    {
      ValentMediaAdapter *adapter = NULL;

      adapter = g_ptr_array_index (media->adapters, i);
      valent_media_adapter_unexport_player (adapter, player);
    }

  g_list_store_remove (G_LIST_STORE (media->exports), position);

  VALENT_EXIT;
}

/**
 * valent_media_pause:
 * @media: a #ValentMedia
 *
 * Pause any playing media players. Any player whose playback status is changed
 * will be tracked so that playback may be resumed with valent_media_play().
 *
 * Since: 1.0
 */
void
valent_media_pause (ValentMedia *media)
{
  ValentMediaPlayer *player;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA (media));

  for (unsigned int i = 0; i < media->players->len; i++)
    {
      player = g_ptr_array_index (media->players, i);

      if (valent_media_player_get_state (player) == VALENT_MEDIA_STATE_PLAYING)
        {
          valent_media_player_pause (player);
          g_ptr_array_add (media->paused, player);
        }
    }

  VALENT_EXIT;
}

/**
 * valent_media_unpause:
 * @media: a #ValentMedia
 *
 * Unpause any media players we previously paused.
 *
 * Since: 1.0
 */
void
valent_media_unpause (ValentMedia *media)
{
  ValentMediaPlayer *player;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA (media));

  for (unsigned int i = 0; i < media->players->len; i++)
    {
      player = g_ptr_array_index (media->players, i);

      if (valent_media_player_get_state (player) == VALENT_MEDIA_STATE_PAUSED)
        valent_media_player_play (player);
    }

  g_ptr_array_remove_range (media->paused, 0, media->paused->len);

  VALENT_EXIT;
}

