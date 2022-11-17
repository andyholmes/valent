// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media"

#include "config.h"

#include <gio/gio.h>
#include <libpeas/peas.h>
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
  GPtrArray       *players;
  GPtrArray       *paused;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMedia, valent_media, VALENT_TYPE_COMPONENT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

enum {
  PLAYER_CHANGED,
  PLAYER_SEEKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentMedia *default_media = NULL;


/*
 * GListModel
 */
static gpointer
valent_media_get_item (GListModel   *list,
                       unsigned int  position)
{
  ValentMedia *self = VALENT_MEDIA (list);

  g_assert (VALENT_IS_MEDIA (self));
  g_assert (position < self->players->len);

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
 * Signal Relays
 */
static void
on_player_changed (ValentMediaPlayer *player,
                   GParamSpec        *pspec,
                   ValentMedia       *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));

  if (g_strcmp0 (pspec->name, "position") == 0)
    {
      double position = 0;

      position = valent_media_player_get_position (player);
      g_signal_emit (G_OBJECT (self), signals [PLAYER_SEEKED], 0, player, position);
    }
  else
    {
      g_signal_emit (G_OBJECT (self), signals [PLAYER_CHANGED], 0, player);
    }

  VALENT_EXIT;
}

static void
on_player_added (ValentMediaAdapter *adapter,
                 ValentMediaPlayer  *player,
                 ValentMedia        *self)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));
  g_assert (VALENT_IS_MEDIA (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (player),
               valent_media_player_get_name (player));

  g_signal_connect_object (player,
                           "notify",
                           G_CALLBACK (on_player_changed),
                           self, 0);

  position = self->players->len;
  g_ptr_array_add (self->players, g_object_ref (player));
  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  VALENT_EXIT;
}

static void
on_player_removed (ValentMediaAdapter *adapter,
                   ValentMediaPlayer  *player,
                   ValentMedia        *self)
{
  unsigned int position = 0;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));
  g_assert (VALENT_IS_MEDIA (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (player),
               valent_media_player_get_name (player));

  g_signal_handlers_disconnect_by_data (player, self);
  g_ptr_array_remove (self->paused, player);

  if (g_ptr_array_find (self->players, player, &position))
    {
      g_ptr_array_remove_index (self->players, position);
      g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
    }

  VALENT_EXIT;
}

/*
 * ValentComponent
 */
static void
valent_media_bind_extension (ValentComponent *component,
                             PeasExtension   *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  ValentMediaAdapter *adapter = VALENT_MEDIA_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));
  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));

  g_ptr_array_add (self->adapters, g_object_ref (adapter));
  g_signal_connect_object (adapter,
                           "player-added",
                           G_CALLBACK (on_player_added),
                           self,
                           0);
  g_signal_connect_object (adapter,
                           "player-removed",
                           G_CALLBACK (on_player_removed),
                           self,
                           0);

  VALENT_EXIT;
}

static void
valent_media_unbind_extension (ValentComponent *component,
                               PeasExtension   *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  ValentMediaAdapter *adapter = VALENT_MEDIA_ADAPTER (extension);
  g_autoptr (GPtrArray) players = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));
  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));

  players = valent_media_adapter_get_players (adapter);

  for (unsigned int i = 0; i < players->len; i++)
    valent_media_adapter_player_removed (adapter, g_ptr_array_index (players, i));

  g_signal_handlers_disconnect_by_func (adapter, on_player_added, self);
  g_signal_handlers_disconnect_by_func (adapter, on_player_removed, self);
  g_ptr_array_remove (self->adapters, extension);

  VALENT_EXIT;
}

/*
 * GObject
 */
static void
valent_media_finalize (GObject *object)
{
  ValentMedia *self = VALENT_MEDIA (object);

  g_clear_pointer (&self->adapters, g_ptr_array_unref);
  g_clear_pointer (&self->players, g_ptr_array_unref);
  g_clear_pointer (&self->paused, g_ptr_array_unref);

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

  /**
   * ValentMedia::player-changed:
   * @media: an #ValentMedia
   * @player: an #ValentMediaPlayer
   *
   * Emitted when the state of a [class@Valent.MediaPlayer] has changed.
   *
   * Since: 1.0
   */
  signals [PLAYER_CHANGED] =
    g_signal_new ("player-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MEDIA_PLAYER);
  g_signal_set_va_marshaller (signals [PLAYER_CHANGED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMedia::player-seeked:
   * @media: an #ValentMedia
   * @player: an #ValentMediaPlayer
   * @offset: the new position in seconds
   *
   * Emitted when the playback position of a [class@Valent.MediaPlayer] has been
   * changed as the result of an external action.
   *
   * Since: 1.0
   */
  signals [PLAYER_SEEKED] =
    g_signal_new ("player-seeked",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2, VALENT_TYPE_MEDIA_PLAYER, G_TYPE_DOUBLE);
}

static void
valent_media_init (ValentMedia *self)
{
  self->adapters = g_ptr_array_new_with_free_func (g_object_unref);
  self->players = g_ptr_array_new_with_free_func (g_object_unref);
  self->paused = g_ptr_array_new ();
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
                                    "plugin-context",  "media",
                                    "plugin-priority", "MediaAdapterPriority",
                                    "plugin-type",     VALENT_TYPE_MEDIA_ADAPTER,
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

  for (unsigned int i = 0; i < media->adapters->len; i++)
    {
      ValentMediaAdapter *adapter = NULL;

      adapter = g_ptr_array_index (media->adapters, i);
      valent_media_adapter_export_player (adapter, player);
    }

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
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA (media));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  for (unsigned int i = 0; i < media->adapters->len; i++)
    {
      ValentMediaAdapter *adapter = NULL;

      adapter = g_ptr_array_index (media->adapters, i);
      valent_media_adapter_unexport_player (adapter, player);
    }

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

