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

  GCancellable    *cancellable;

  GPtrArray       *players;
  GPtrArray       *paused;
};

G_DEFINE_TYPE (ValentMedia, valent_media, VALENT_TYPE_COMPONENT)

enum {
  PLAYER_ADDED,
  PLAYER_CHANGED,
  PLAYER_REMOVED,
  PLAYER_SEEKED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

static ValentMedia *default_media = NULL;


/*
 * Signal Relays
 */
static void
on_player_changed (ValentMediaPlayer *player,
                   ValentMedia       *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));

  g_signal_emit (G_OBJECT (self), signals [PLAYER_CHANGED], 0, player);

  VALENT_EXIT;
}

static void
on_player_seeked (ValentMediaPlayer *player,
                  gint64             offset,
                  ValentMedia       *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));

  g_signal_emit (G_OBJECT (self), signals [PLAYER_SEEKED], 0, player, offset);

  VALENT_EXIT;
}

static void
on_player_added (ValentMediaAdapter *adapter,
                 ValentMediaPlayer  *player,
                 ValentMedia        *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (player),
               valent_media_player_get_name (player));

  g_signal_connect_object (player,
                           "changed",
                           G_CALLBACK (on_player_changed),
                           self, 0);

  g_signal_connect_object (player,
                           "seeked",
                           G_CALLBACK (on_player_seeked),
                           self, 0);

  g_ptr_array_add (self->players, g_object_ref (player));
  g_signal_emit (G_OBJECT (self), signals [PLAYER_ADDED], 0, player);

  VALENT_EXIT;
}

static void
on_player_removed (ValentMediaAdapter *adapter,
                   ValentMediaPlayer  *player,
                   ValentMedia        *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));

  VALENT_NOTE ("%s: %s",
               G_OBJECT_TYPE_NAME (player),
               valent_media_player_get_name (player));

  g_signal_handlers_disconnect_by_func (player, on_player_changed, self);
  g_signal_handlers_disconnect_by_func (player, on_player_seeked, self);

  g_ptr_array_remove (self->players, player);
  g_ptr_array_remove (self->paused, player);

  g_signal_emit (G_OBJECT (self), signals [PLAYER_REMOVED], 0, player);

  VALENT_EXIT;
}

static void
valent_media_adapter_load_cb (ValentMediaAdapter *adapter,
                              GAsyncResult       *result,
                              ValentMedia        *self)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));

  if (!valent_media_adapter_load_finish (adapter, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s failed to load: %s", G_OBJECT_TYPE_NAME (adapter), error->message);

  VALENT_EXIT;
}


/*
 * ValentComponent
 */
static void
valent_media_extension_added (ValentComponent *component,
                              PeasExtension   *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  ValentMediaAdapter *adapter = VALENT_MEDIA_ADAPTER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_COMPONENT (component));
  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (VALENT_IS_MEDIA (self));

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

  valent_media_adapter_load_async (adapter,
                                   self->cancellable,
                                   (GAsyncReadyCallback)valent_media_adapter_load_cb,
                                   self);

  VALENT_EXIT;
}

static void
valent_media_extension_removed (ValentComponent *component,
                                PeasExtension   *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  ValentMediaAdapter *adapter = VALENT_MEDIA_ADAPTER (extension);
  g_autoptr (GPtrArray) players = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_COMPONENT (component));
  g_assert (VALENT_IS_MEDIA (component));

  players = valent_media_adapter_get_players (adapter);

  for (unsigned int i = 0; i < players->len; i++)
    valent_media_adapter_emit_player_removed (adapter, g_ptr_array_index (players, i));

  g_signal_handlers_disconnect_by_data (adapter, self);

  VALENT_EXIT;
}


/*
 * GObject
 */
static void
valent_media_dispose (GObject *object)
{
  ValentMedia *self = VALENT_MEDIA (object);

  if (!g_cancellable_is_cancelled (self->cancellable))
    g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (valent_media_parent_class)->dispose (object);
}

static void
valent_media_finalize (GObject *object)
{
  ValentMedia *self = VALENT_MEDIA (object);

  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->players, g_ptr_array_unref);
  g_clear_pointer (&self->paused, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_media_parent_class)->finalize (object);
}

static void
valent_media_class_init (ValentMediaClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ValentComponentClass *component_class = VALENT_COMPONENT_CLASS (klass);

  object_class->dispose = valent_media_dispose;
  object_class->finalize = valent_media_finalize;

  component_class->extension_added = valent_media_extension_added;
  component_class->extension_removed = valent_media_extension_removed;


  /**
   * ValentMedia::player-added:
   * @media: an #ValentMedia
   * @player: an #ValentMediaPlayer
   *
   * Emitted when a [class@Valent.MediaPlayer] has been added to a
   * [class@Valent.MediaAdapter] implementation.
   *
   * Since: 1.0
   */
  signals [PLAYER_ADDED] =
    g_signal_new ("player-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MEDIA_PLAYER);
  g_signal_set_va_marshaller (signals [PLAYER_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMedia::player-removed:
   * @media: an #ValentMedia
   * @player: an #ValentMediaPlayer
   *
   * Emitted when a [class@Valent.MediaPlayer] has been removed from a
   * [class@Valent.MediaAdapter] implementation.
   *
   * Since: 1.0
   */
  signals [PLAYER_REMOVED] =
    g_signal_new ("player-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0,
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MEDIA_PLAYER);
  g_signal_set_va_marshaller (signals [PLAYER_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

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
   * @offset: the relative change in microseconds (us)
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
                  G_TYPE_NONE, 2, VALENT_TYPE_MEDIA_PLAYER, G_TYPE_INT64);
}

static void
valent_media_init (ValentMedia *self)
{
  self->cancellable = g_cancellable_new ();
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
                                    "plugin-context", "media",
                                    "plugin-type",    VALENT_TYPE_MEDIA_ADAPTER,
                                    NULL);

      g_object_add_weak_pointer (G_OBJECT (default_media),
                                 (gpointer)&default_media);
    }

  return default_media;
}

/**
 * valent_media_get_players:
 * @media: a #ValentMedia
 *
 * Get a list of all the #ValentMediaPlayer instances currently known to @media.
 *
 * Returns: (transfer container) (element-type Valent.MediaPlayer) (not nullable):
 *  a #GPtrArray of #ValentMediaPlayer
 *
 * Since: 1.0
 */
GPtrArray *
valent_media_get_players (ValentMedia *media)
{
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA (media), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < media->players->len; i++)
    g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (media->players, i)));

  VALENT_RETURN (ret);
}

/**
 * valent_media_get_player_by_name:
 * @media: a #ValentMedia
 * @name: player name
 *
 * Get the #ValentMediaPlayer with the identity @name.
 *
 * Returns: (transfer none) (nullable): a #ValentMediaPlayer
 *
 * Since: 1.0
 */
ValentMediaPlayer *
valent_media_get_player_by_name (ValentMedia *media,
                                 const char  *name)
{
  ValentMediaPlayer *player;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA (media), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (unsigned int i = 0; i < media->players->len; i++)
    {
      player = g_ptr_array_index (media->players, i);

      if (g_strcmp0 (valent_media_player_get_name (player), name) == 0)
        VALENT_RETURN (player);
    }

  VALENT_RETURN (NULL);
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

      if (valent_media_player_is_playing (player))
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

      if (!valent_media_player_is_playing (player))
        valent_media_player_play (player);
    }

  g_ptr_array_remove_range (media->paused, 0, media->paused->len);

  VALENT_EXIT;
}

