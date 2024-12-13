// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media"

#include "config.h"

#include <gio/gio.h>
#include <libpeas.h>
#include <libvalent-core.h>

#include "valent-media-adapter.h"
#include "valent-media-player.h"

#include "valent-media.h"

/**
 * ValentMedia:
 *
 * A class for monitoring and controlling media players.
 *
 * `ValentMedia` is an aggregator of media players, intended for use by
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

  GPtrArray       *exports;
};

G_DEFINE_FINAL_TYPE (ValentMedia, valent_media, VALENT_TYPE_COMPONENT)

/*
 * GObject
 */
static void
valent_media_finalize (GObject *object)
{
  ValentMedia *self = VALENT_MEDIA (object);

  g_clear_pointer (&self->exports, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_media_parent_class)->finalize (object);
}

static void
valent_media_class_init (ValentMediaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_media_finalize;
}

static void
valent_media_init (ValentMedia *self)
{
  self->exports = g_ptr_array_new_with_free_func (g_object_unref);
}

/**
 * valent_media_get_default:
 *
 * Get the default [class@Valent.Media].
 *
 * Returns: (transfer none) (not nullable): a `ValentMedia`
 *
 * Since: 1.0
 */
ValentMedia *
valent_media_get_default (void)
{
  static ValentMedia *default_instance = NULL;

  if (default_instance == NULL)
    {
      default_instance = g_object_new (VALENT_TYPE_MEDIA,
                                       "plugin-domain", "media",
                                       "plugin-type",   VALENT_TYPE_MEDIA_ADAPTER,
                                       NULL);
      g_object_add_weak_pointer (G_OBJECT (default_instance),
                                 (gpointer)&default_instance);
    }

  return default_instance;
}

/**
 * valent_media_export_player:
 * @media: a `ValentMedia`
 * @player: a `ValentMediaPlayer`
 *
 * Export @player on all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_media_export_player (ValentMedia       *media,
                            ValentMediaPlayer *player)
{
  ValentResource *source = NULL;
  unsigned int n_items;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA (media));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  if (g_ptr_array_find (media->exports, player, NULL))
    {
      g_warning ("Player \"%s\" (%s) already exported",
                 valent_media_player_get_name (player),
                 G_OBJECT_TYPE_NAME (player));
      VALENT_EXIT;
    }

  g_signal_connect_object (player,
                           "destroy",
                           G_CALLBACK (valent_media_unexport_player),
                           media,
                           G_CONNECT_SWAPPED);
  g_ptr_array_add (media->exports, g_object_ref (player));

  source = valent_resource_get_source (VALENT_RESOURCE (player));
  n_items = g_list_model_get_n_items (G_LIST_MODEL (media));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (ValentMediaAdapter) adapter = NULL;

      adapter = g_list_model_get_item (G_LIST_MODEL (media), i);
      if (VALENT_RESOURCE (adapter) != source)
        valent_media_adapter_export_player (adapter, player);
    }

  VALENT_EXIT;
}

/**
 * valent_media_unexport_player:
 * @media: a `ValentMedia`
 * @player: a `ValentMediaPlayer`
 *
 * Unexport @player from all adapters that support it.
 *
 * Since: 1.0
 */
void
valent_media_unexport_player (ValentMedia       *media,
                              ValentMediaPlayer *player)
{
  ValentResource *source = NULL;
  unsigned int n_items;
  g_autoptr (ValentExtension) item = NULL;
  unsigned int position = 0;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA (media));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  if (!g_ptr_array_find (media->exports, player, &position))
    {
      g_critical ("%s(): unknown player %s (%s)",
                  G_STRFUNC,
                  G_OBJECT_TYPE_NAME (player),
                  valent_media_player_get_name (player));
      VALENT_EXIT;
    }

  g_signal_handlers_disconnect_by_func (player, valent_media_unexport_player, media);
  item = g_ptr_array_steal_index (media->exports, position);

  source = valent_resource_get_source (VALENT_RESOURCE (player));
  n_items = g_list_model_get_n_items (G_LIST_MODEL (media));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (ValentMediaAdapter) adapter = NULL;

      adapter = g_list_model_get_item (G_LIST_MODEL (media), i);
      if (VALENT_RESOURCE (adapter) != source)
        valent_media_adapter_unexport_player (adapter, player);
    }

  VALENT_EXIT;
}

