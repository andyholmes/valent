// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media-adapter"

#include "config.h"

#include <libvalent-core.h>

#include "valent-media-player.h"

#include "valent-media-adapter.h"

/**
 * ValentMediaAdapter:
 *
 * An abstract base class for media player adapters.
 *
 * `ValentMediaAdapter` is a base class for plugins that provide an interface to
 * manage media players. This usually means monitoring and querying instances of
 * [class@Valent.MediaPlayer].
 *
 * ## `.plugin` File
 *
 * Implementations may define the following extra fields in the `.plugin` file:
 *
 * - `X-MediaAdapterPriority`
 *
 *     An integer indicating the adapter priority. The implementation with the
 *     lowest value will be used as the primary adapter.
 *
 * Since: 1.0
 */
typedef struct
{
  GPtrArray *items;
} ValentMediaAdapterPrivate;

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ValentMediaAdapter, valent_media_adapter, VALENT_TYPE_EXTENSION,
                                  G_ADD_PRIVATE (ValentMediaAdapter)
                                  G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

static inline void
_valent_object_deref (gpointer data)
{
  if (!valent_object_in_destruction (VALENT_OBJECT (data)))
    valent_object_destroy (VALENT_OBJECT (data));

  g_object_unref (data);
}

/*
 * GListModel
 */
static gpointer
valent_media_adapter_get_item (GListModel   *list,
                               unsigned int  position)
{
  ValentMediaAdapter *self = VALENT_MEDIA_ADAPTER (list);
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (self);

  g_assert (VALENT_IS_MEDIA_ADAPTER (self));

  if G_UNLIKELY (position >= priv->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (priv->items, position));
}

static GType
valent_media_adapter_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MEDIA_PLAYER;
}

static unsigned int
valent_media_adapter_get_n_items (GListModel *list)
{
  ValentMediaAdapter *self = VALENT_MEDIA_ADAPTER (list);
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (self);

  g_assert (VALENT_IS_MEDIA_ADAPTER (self));

  return priv->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_media_adapter_get_item;
  iface->get_item_type = valent_media_adapter_get_item_type;
  iface->get_n_items = valent_media_adapter_get_n_items;
}

/* LCOV_EXCL_START */
static void
valent_media_adapter_real_export_player (ValentMediaAdapter *adapter,
                                         ValentMediaPlayer  *player)
{
  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));
}

static void
valent_media_adapter_real_unexport_player (ValentMediaAdapter *adapter,
                                           ValentMediaPlayer  *player)
{
  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_media_adapter_finalize (GObject *object)
{
  ValentMediaAdapter *self = VALENT_MEDIA_ADAPTER (object);
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (self);

  g_clear_pointer (&priv->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_media_adapter_parent_class)->finalize (object);
}

static void
valent_media_adapter_class_init (ValentMediaAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_media_adapter_finalize;

  klass->export_player = valent_media_adapter_real_export_player;
  klass->unexport_player = valent_media_adapter_real_unexport_player;
}

static void
valent_media_adapter_init (ValentMediaAdapter *self)
{
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (self);

  priv->items = g_ptr_array_new_with_free_func (_valent_object_deref);
}

/**
 * valent_media_adapter_player_added:
 * @adapter: a `ValentMediaAdapter`
 * @player: a `ValentMediaPlayer`
 *
 * Called when @player has been added to @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.MediaAdapter]. @adapter will hold a reference on @player and
 * emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_media_adapter_player_added (ValentMediaAdapter *adapter,
                                   ValentMediaPlayer  *player)
{
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (adapter);
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  if (g_ptr_array_find (priv->items, player, &position))
    {
      g_warning ("Player \"%s\" (%s) already exported",
                 valent_media_player_get_name (player),
                 G_OBJECT_TYPE_NAME (player));
      return;
    }

  g_signal_connect_object (player,
                           "destroy",
                           G_CALLBACK (valent_media_adapter_player_removed),
                           adapter,
                           G_CONNECT_SWAPPED);

  position = priv->items->len;
  g_ptr_array_add (priv->items, g_object_ref (player));
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 0, 1);
}

/**
 * valent_media_adapter_player_removed:
 * @adapter: a `ValentMediaAdapter`
 * @player: a `ValentMediaPlayer`
 *
 * Called when @player has been removed from @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.MediaAdapter]. @adapter will drop its reference on @player
 * and emit [signal@Gio.ListModel::items-changed].
 *
 * Since: 1.0
 */
void
valent_media_adapter_player_removed (ValentMediaAdapter *adapter,
                                     ValentMediaPlayer  *player)
{
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (adapter);
  unsigned int position = 0;

  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  if (!g_ptr_array_find (priv->items, player, &position))
    {
      g_warning ("No such player \"%s\" found in \"%s\"",
                 G_OBJECT_TYPE_NAME (player),
                 G_OBJECT_TYPE_NAME (adapter));
      return;
    }

  g_signal_handlers_disconnect_by_func (player, valent_media_adapter_player_removed, adapter);
  g_ptr_array_remove_index (priv->items, position);
  g_list_model_items_changed (G_LIST_MODEL (adapter), position, 1, 0);
}

/**
 * valent_media_adapter_export_player: (virtual export_player)
 * @adapter: an `ValentMediaAdapter`
 * @player: a `ValentMediaPlayer`
 *
 * Export @player on @adapter.
 *
 * This method is intended to allow device plugins to expose remote media
 * players to the host system. Usually this means exporting an interface on
 * D-Bus or an mDNS service.
 *
 * Implementations must automatically unexport any players when destroyed.
 *
 * Since: 1.0
 */
void
valent_media_adapter_export_player (ValentMediaAdapter *adapter,
                                    ValentMediaPlayer  *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_ADAPTER_GET_CLASS (adapter)->export_player (adapter, player);

  VALENT_EXIT;
}

/**
 * valent_media_adapter_unexport_player: (virtual unexport_player)
 * @adapter: an `ValentMediaAdapter`
 * @player: a `ValentMediaPlayer`
 *
 * Unexport @player from @adapter.
 *
 * Since: 1.0
 */
void
valent_media_adapter_unexport_player (ValentMediaAdapter *adapter,
                                      ValentMediaPlayer  *player)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  VALENT_MEDIA_ADAPTER_GET_CLASS (adapter)->unexport_player (adapter, player);

  VALENT_EXIT;
}

