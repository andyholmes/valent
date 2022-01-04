// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media-player-provider"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-media-player.h"
#include "valent-media-player-provider.h"


/**
 * SECTION:valentmediaplayerprovider
 * @short_description: Base class for media player providers
 * @title: ValentMediaPlayerProvider
 * @stability: Unstable
 * @include: libvalent-media.h
 *
 * #ValentMediaPlayerProvider is base class for services that provide #ValentMediaPlayer objects.
 */

typedef struct
{
  PeasPluginInfo *plugin_info;

  GPtrArray      *players;
} ValentMediaPlayerProviderPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentMediaPlayerProvider, valent_media_player_provider, G_TYPE_OBJECT)

/**
 * ValentMediaPlayerProviderClass:
 * @load_async: the virtual function pointer for valent_media_player_provider_load_async()
 * @load_finish: the virtual function pointer for valent_media_player_provider_load_finish()
 * @player_added: the class closure for #ValentMediaPlayerProvider::player-added signal
 * @player_removed: the class closure for #ValentMediaPlayerProvider:player-removed signal
 *
 * The virtual function table for #ValentMediaPlayerProvider.
 */

enum {
  PROP_0,
  PROP_PLUGIN_INFO,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  PLAYER_ADDED,
  PLAYER_REMOVED,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };

/* LCOV_EXCL_START */
static void
valent_media_player_provider_real_load_async (ValentMediaPlayerProvider *provider,
                                              GCancellable              *cancellable,
                                              GAsyncReadyCallback        callback,
                                              gpointer                   user_data)
{
  g_task_report_new_error (provider, callback, user_data,
                           valent_media_player_provider_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (provider));
}

static gboolean
valent_media_player_provider_real_load_finish (ValentMediaPlayerProvider  *provider,
                                               GAsyncResult               *result,
                                               GError                    **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
valent_media_player_provider_real_player_added (ValentMediaPlayerProvider *provider,
                                                ValentMediaPlayer         *player)
{
  ValentMediaPlayerProviderPrivate *priv = valent_media_player_provider_get_instance_private (provider);

  g_assert (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  if (priv->players == NULL)
    priv->players = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->players, g_object_ref (player));
}

static void
valent_media_player_provider_real_player_removed (ValentMediaPlayerProvider *provider,
                                                  ValentMediaPlayer         *player)
{
  ValentMediaPlayerProviderPrivate *priv = valent_media_player_provider_get_instance_private (provider);

  g_assert (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  /* Maybe we just disposed */
  if (priv->players == NULL)
    return;

  if (!g_ptr_array_remove (priv->players, player))
    g_warning ("No such media player \"%s\" found in \"%s\"",
               G_OBJECT_TYPE_NAME (player),
               G_OBJECT_TYPE_NAME (provider));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_media_player_provider_dispose (GObject *object)
{
  ValentMediaPlayerProvider *self = VALENT_MEDIA_PLAYER_PROVIDER (object);
  ValentMediaPlayerProviderPrivate *priv = valent_media_player_provider_get_instance_private (self);

  g_clear_pointer (&priv->players, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_media_player_provider_parent_class)->dispose (object);
}

static void
valent_media_player_provider_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  ValentMediaPlayerProvider *self = VALENT_MEDIA_PLAYER_PROVIDER (object);
  ValentMediaPlayerProviderPrivate *priv = valent_media_player_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      g_value_set_boxed (value, priv->plugin_info);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_media_player_provider_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  ValentMediaPlayerProvider *self = VALENT_MEDIA_PLAYER_PROVIDER (object);
  ValentMediaPlayerProviderPrivate *priv = valent_media_player_provider_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_PLUGIN_INFO:
      priv->plugin_info = g_value_get_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_media_player_provider_class_init (ValentMediaPlayerProviderClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_media_player_provider_dispose;
  object_class->get_property = valent_media_player_provider_get_property;
  object_class->set_property = valent_media_player_provider_set_property;

  klass->player_added = valent_media_player_provider_real_player_added;
  klass->player_removed = valent_media_player_provider_real_player_removed;
  klass->load_async = valent_media_player_provider_real_load_async;
  klass->load_finish = valent_media_player_provider_real_load_finish;

  /**
   * ValentMediaPlayerProvider:plugin-info:
   *
   * The #PeasPluginInfo describing this provider.
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "Plugin Info",
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentMediaPlayerProvider::player-added:
   * @provider: an #ValentMediaPlayerProvider
   * @player: an #ValentMediaPlayer
   *
   * The "player-added" signal is emitted when a provider has discovered a
   * player has become available.
   *
   * Subclasses of #ValentMediaPlayerManager must chain-up if they override the
   * #ValentMediaPlayerProviderClass.player_added vfunc.
   */
  signals [PLAYER_ADDED] =
    g_signal_new ("player-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentMediaPlayerProviderClass, player_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MEDIA_PLAYER);
  g_signal_set_va_marshaller (signals [PLAYER_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMediaPlayerProvider::player-removed:
   * @provider: an #ValentMediaPlayerProvider
   * @player: an #ValentMediaPlayer
   *
   * The "player-removed" signal is emitted when a provider has discovered a
   * player is no longer available.
   *
   * Subclasses of #ValentMediaPlayerManager must chain-up if they override the
   * #ValentMediaPlayerProviderClass.player_removed vfunc.
   */
  signals [PLAYER_REMOVED] =
    g_signal_new ("player-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentMediaPlayerProviderClass, player_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MEDIA_PLAYER);
  g_signal_set_va_marshaller (signals [PLAYER_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_media_player_provider_init (ValentMediaPlayerProvider *provider)
{
}

/**
 * valent_media_player_provider_emit_player_added:
 * @provider: a #ValentMediaPlayerProvider
 * @player: a #ValentMediaPlayer
 *
 * Emits the #ValentMediaPlayerProvider::player-added signal.
 *
 * This should only be called by subclasses of #ValentMediaPlayerProvider when a new media player
 * has been discovered.
 */
void
valent_media_player_provider_emit_player_added (ValentMediaPlayerProvider *provider,
                                                ValentMediaPlayer         *player)
{
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_signal_emit (G_OBJECT (provider), signals [PLAYER_ADDED], 0, player);
}

/**
 * valent_media_player_provider_emit_player_removed:
 * @provider: a #ValentMediaPlayerProvider
 * @player: a #ValentMediaPlayer
 *
 * Emits the #ValentMediaPlayerProvider::player-removed signal.
 *
 * This should only be called by subclasses of #ValentMediaPlayerProvider when a previously added
 * media player has been removed.
 */
void
valent_media_player_provider_emit_player_removed (ValentMediaPlayerProvider *provider,
                                                  ValentMediaPlayer         *player)
{
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_signal_emit (G_OBJECT (provider), signals [PLAYER_REMOVED], 0, player);
}

/**
 * valent_media_player_provider_load_async:
 * @provider: an #ValentMediaPlayerProvider
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Requests that the #ValentMediaPlayerProvider asynchronously load any known players.
 *
 * This should only be called once on an #ValentMediaPlayerProvider. It is an error
 * to call this function more than once for a single #ValentMediaPlayerProvider.
 *
 * #ValentMediaPlayerProvider implementations are expected to emit the
 * #ValentMediaPlayerProvider::media_player-added signal for each media_player they've discovered.
 * That should be done for known players before returning from the asynchronous
 * operation so that the media_player manager does not need to wait for additional
 * players to enter the "settled" state.
 */
void
valent_media_player_provider_load_async (ValentMediaPlayerProvider *provider,
                                         GCancellable              *cancellable,
                                         GAsyncReadyCallback        callback,
                                         gpointer                   user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_MEDIA_PLAYER_PROVIDER_GET_CLASS (provider)->load_async (provider,
                                                                 cancellable,
                                                                 callback,
                                                                 user_data);

  VALENT_EXIT;
}

/**
 * valent_media_player_provider_load_finish:
 * @provider: an #ValentMediaPlayerProvider
 * @result: a #GAsyncResult provided to callback
 * @error: (nullable): a #GError
 *
 * Completes an asynchronous request to load known players via
 * valent_media_player_provider_load_async().
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 */
gboolean
valent_media_player_provider_load_finish (ValentMediaPlayerProvider  *provider,
                                          GAsyncResult               *result,
                                          GError                    **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, provider), FALSE);

  ret = VALENT_MEDIA_PLAYER_PROVIDER_GET_CLASS (provider)->load_finish (provider, result, error);

  VALENT_RETURN (ret);
}

/**
 * valent_media_player_provider_get_players:
 * @provider: an #ValentMediaPlayerProvider
 *
 * Gets a new #GPtrArray containing a list of #ValentMediaPlayer instances that
 * were registered by @provider.
 *
 * Returns: (transfer container) (element-type Valent.MediaPlayer): a list of
 *   players.
 */
GPtrArray *
valent_media_player_provider_get_players (ValentMediaPlayerProvider *provider)
{
  ValentMediaPlayerProviderPrivate *priv = valent_media_player_provider_get_instance_private (provider);
  g_autoptr (GPtrArray) players = NULL;

  g_return_val_if_fail (VALENT_IS_MEDIA_PLAYER_PROVIDER (provider), NULL);

  players = g_ptr_array_new_with_free_func (g_object_unref);

  if (priv->players != NULL)
    {
      for (unsigned int i = 0; i < priv->players->len; i++)
        g_ptr_array_add (players, g_object_ref (g_ptr_array_index (priv->players, i)));
    }

  return g_steal_pointer (&players);
}

