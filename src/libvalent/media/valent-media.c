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

  /* list model */
  GPtrArray       *items;
};

static void   valent_media_unbind_extension (ValentComponent *component,
                                             GObject         *extension);
static void   g_list_model_iface_init       (GListModelInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (ValentMedia, valent_media, VALENT_TYPE_COMPONENT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

static ValentMedia *default_media = NULL;

/*
 * ValentComponent
 */
static void
valent_media_bind_extension (ValentComponent *component,
                             GObject         *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));
  g_assert (VALENT_IS_MEDIA_ADAPTER (extension));

  if (g_ptr_array_find (self->items, extension, NULL))
    {
      g_warning ("Adapter \"%s\" already exported in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_signal_connect_object (extension,
                           "destroy",
                           G_CALLBACK (valent_media_unbind_extension),
                           self,
                           G_CONNECT_SWAPPED);

  position = self->items->len;
  g_ptr_array_add (self->items, g_object_ref (extension));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  VALENT_EXIT;
}

static void
valent_media_unbind_extension (ValentComponent *component,
                               GObject         *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  g_autoptr (ValentExtension) item = NULL;
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));
  g_assert (VALENT_IS_MEDIA_ADAPTER (extension));

  if (!g_ptr_array_find (self->items, extension, &position))
    {
      g_warning ("Adapter \"%s\" not found in \"%s\"",
                 G_OBJECT_TYPE_NAME (extension),
                 G_OBJECT_TYPE_NAME (component));
      return;
    }

  g_signal_handlers_disconnect_by_func (extension, valent_media_unbind_extension, self);
  item = g_ptr_array_steal_index (self->items, position);
  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);

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

  if G_UNLIKELY (position >= self->items->len)
    return NULL;

  return g_object_ref (g_ptr_array_index (self->items, position));
}

static GType
valent_media_get_item_type (GListModel *list)
{
  return VALENT_TYPE_MEDIA_ADAPTER;
}

static unsigned int
valent_media_get_n_items (GListModel *list)
{
  ValentMedia *self = VALENT_MEDIA (list);

  g_assert (VALENT_IS_MEDIA (self));

  return self->items->len;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_media_get_item;
  iface->get_item_type = valent_media_get_item_type;
  iface->get_n_items = valent_media_get_n_items;
}

/*
 * GObject
 */
static void
valent_media_finalize (GObject *object)
{
  ValentMedia *self = VALENT_MEDIA (object);

  g_clear_pointer (&self->exports, g_ptr_array_unref);
  g_clear_pointer (&self->items, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_media_parent_class)->finalize (object);
}

static void
valent_media_class_init (ValentMediaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->finalize = valent_media_finalize;

  component_class->bind_extension = valent_media_bind_extension;
  component_class->unbind_extension = valent_media_unbind_extension;
}

static void
valent_media_init (ValentMedia *self)
{
  self->items = g_ptr_array_new_with_free_func (g_object_unref);
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
  for (unsigned int i = 0; i < media->items->len; i++)
    {
      ValentMediaAdapter *adapter = g_ptr_array_index (media->items, i);

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
  for (unsigned int i = 0; i < media->items->len; i++)
    {
      ValentMediaAdapter *adapter = g_ptr_array_index (media->items, i);

      if (VALENT_RESOURCE (adapter) != source)
        valent_media_adapter_unexport_player (adapter, player);
    }

  VALENT_EXIT;
}

