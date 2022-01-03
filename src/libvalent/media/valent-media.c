// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-media.h"
#include "valent-media-player.h"
#include "valent-media-player-provider.h"


/**
 * SECTION:valentmedia
 * @short_description: Media Abstraction
 * @title: ValentMedia
 * @stability: Unstable
 * @include: libvalent-media.h
 *
 * #ValentMedia is an aggregator of desktop media players, with a simple API
 * generally intended to be used by #ValentDevicePlugin implementations.
 *
 * Plugins can provide adapters for media players by subclassing the
 * #ValentMediaPlayerProvider base class to register #ValentMediaPlayer
 * instances by emitting #ValentMediaPlayerProvider::player-added.
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
on_player_added (ValentMediaPlayerProvider *provider,
                 ValentMediaPlayer         *player,
                 ValentMedia               *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));

  VALENT_DEBUG ("%s: %s", G_OBJECT_TYPE_NAME (player), valent_media_player_get_name (player));

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
on_player_removed (ValentMediaPlayerProvider *provider,
                   ValentMediaPlayer         *player,
                   ValentMedia               *self)
{
  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA (self));

  VALENT_DEBUG ("%s: %s", G_OBJECT_TYPE_NAME (player), valent_media_player_get_name (player));

  g_signal_handlers_disconnect_by_func (player, on_player_changed, self);
  g_signal_handlers_disconnect_by_func (player, on_player_seeked, self);

  g_ptr_array_remove (self->players, player);
  g_ptr_array_remove (self->paused, player);

  g_signal_emit (G_OBJECT (self), signals [PLAYER_REMOVED], 0, player);

  VALENT_EXIT;
}

static void
valent_media_player_provider_load_cb (ValentMediaPlayerProvider *provider,
                                      GAsyncResult              *result,
                                      ValentMedia               *self)
{
  g_autoptr (GError) error = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider));
  g_assert (g_task_is_valid (result, provider));

  if (!valent_media_player_provider_load_finish (provider, result, &error) &&
      !valent_error_ignore (error))
    g_warning ("%s failed to load: %s", G_OBJECT_TYPE_NAME (provider), error->message);

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
  ValentMediaPlayerProvider *provider = VALENT_MEDIA_PLAYER_PROVIDER (extension);

  VALENT_ENTRY;

  g_assert (VALENT_IS_COMPONENT (component));
  g_assert (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider));
  g_assert (VALENT_IS_MEDIA (self));

  g_signal_connect_object (provider,
                           "player-added",
                           G_CALLBACK (on_player_added),
                           self,
                           0);

  g_signal_connect_object (provider,
                           "player-removed",
                           G_CALLBACK (on_player_removed),
                           self,
                           0);

  valent_media_player_provider_load_async (provider,
                                           self->cancellable,
                                           (GAsyncReadyCallback)valent_media_player_provider_load_cb,
                                           self);

  VALENT_EXIT;
}

static void
valent_media_extension_removed (ValentComponent *component,
                                PeasExtension   *extension)
{
  ValentMedia *self = VALENT_MEDIA (component);
  ValentMediaPlayerProvider *provider = VALENT_MEDIA_PLAYER_PROVIDER (extension);
  g_autoptr (GPtrArray) players = NULL;

  VALENT_ENTRY;

  g_assert (VALENT_IS_COMPONENT (component));
  g_assert (VALENT_IS_MEDIA (component));

  players = valent_media_player_provider_get_players (provider);

  for (unsigned int i = 0; i < players->len; i++)
    valent_media_player_provider_emit_player_removed (provider, g_ptr_array_index (players, i));

  g_signal_handlers_disconnect_by_data (provider, self);

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
   * @self: an #ValentMedia
   * @player: an #ValentMediaPlayer
   *
   * The "player-added" signal is emitted when a provider has discovered a
   * player has become available.
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
   * @self: an #ValentMedia
   * @player: an #ValentMediaPlayer
   *
   * The "player-removed" signal is emitted when a provider has discovered a
   * player is no longer available.
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
   * @media: a #ValentMedia
   * @player: a #GDBusProxy
   *
   * The #ValentMedia::player-changed signal is emitted when an MPRIS player's
   * state changes in some way.
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
   * @media: a #ValentMedia
   * @player: a #ValentMediaPlayer
   * @offset: an offset
   *
   * The #ValentMedia::player-seeked signal is emitted when an MPRIS player's
   * position is changed by explicit action.
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
 * Get the default #ValentMedia.
 *
 * Returns: (transfer none): The default media
 */
ValentMedia *
valent_media_get_default (void)
{
  if (default_media == NULL)
    {
      default_media = g_object_new (VALENT_TYPE_MEDIA,
                                    "plugin-context", "media",
                                    "plugin-type",    VALENT_TYPE_MEDIA_PLAYER_PROVIDER,
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
 */
GPtrArray *
valent_media_get_players (ValentMedia *media)
{
  GPtrArray *players;

  g_return_val_if_fail (VALENT_IS_MEDIA (media), NULL);

  players = g_ptr_array_new_with_free_func (g_object_unref);

  for (unsigned int i = 0; i < media->players->len; i++)
    g_ptr_array_add (players, g_object_ref (g_ptr_array_index (media->players, i)));

  return players;
}

/**
 * valent_media_get_player_by_name:
 * @media: a #ValentMedia
 * @name: player name
 *
 * Get the #ValentMediaPlayer with the identity @name.
 *
 * Returns: (transfer none) (nullable): a #ValentMediaPlayer
 */
ValentMediaPlayer *
valent_media_get_player_by_name (ValentMedia *media,
                                 const char  *name)
{
  ValentMediaPlayer *player;

  g_return_val_if_fail (VALENT_IS_MEDIA (media), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  for (unsigned int i = 0; i < media->players->len; i++)
    {
      player = g_ptr_array_index (media->players, i);

      if (g_strcmp0 (valent_media_player_get_name (player), name) == 0)
        return player;
    }

  return NULL;
}

/**
 * valent_media_pause:
 * @media: a #ValentMedia
 *
 * Pause any playing media players. Any player whose playback status is changed
 * will be tracked so that playback may be resumed with valent_media_play().
 */
void
valent_media_pause (ValentMedia *media)
{
  ValentMediaPlayer *player;

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
}

/**
 * valent_media_unpause:
 * @media: a #ValentMedia
 *
 * Unpause any media players we previously paused.
 */
void
valent_media_unpause (ValentMedia *media)
{
  ValentMediaPlayer *player;

  g_return_if_fail (VALENT_IS_MEDIA (media));

  for (unsigned int i = 0; i < media->players->len; i++)
    {
      player = g_ptr_array_index (media->players, i);

      if (!valent_media_player_is_playing (player))
        valent_media_player_play (player);
    }

  g_ptr_array_remove_range (media->paused, 0, media->paused->len);
}

