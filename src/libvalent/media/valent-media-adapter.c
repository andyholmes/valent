// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-media-adapter"

#include "config.h"

#include <libpeas/peas.h>
#include <libvalent-core.h>

#include "valent-media-player.h"
#include "valent-media-adapter.h"


/**
 * ValentMediaAdapter:
 *
 * An abstract base class for media player adapters.
 *
 * #ValentMediaAdapter is a base class for plugins that provide an interface to
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
  PeasPluginInfo *plugin_info;
  GPtrArray      *players;
} ValentMediaAdapterPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentMediaAdapter, valent_media_adapter, G_TYPE_OBJECT)

/**
 * ValentMediaAdapterClass:
 * @export_player: the virtual function pointer for valent_media_adapter_export()
 * @unexport_player: the virtual function pointer for valent_media_adapter_unexport()
 * @player_added: the class closure for #ValentMediaAdapter::player-added signal
 * @player_removed: the class closure for #ValentMediaAdapter:player-removed signal
 *
 * The virtual function table for #ValentMediaAdapter.
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

static void
valent_media_adapter_real_player_added (ValentMediaAdapter *adapter,
                                        ValentMediaPlayer  *player)
{
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  if (priv->players == NULL)
    priv->players = g_ptr_array_new_with_free_func (g_object_unref);
  g_ptr_array_add (priv->players, g_object_ref (player));
}

static void
valent_media_adapter_real_player_removed (ValentMediaAdapter *adapter,
                                          ValentMediaPlayer  *player)
{
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (adapter);

  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (VALENT_IS_MEDIA_PLAYER (player));

  /* Maybe we just disposed */
  if (priv->players == NULL)
    return;

  if (!g_ptr_array_remove (priv->players, player))
    g_warning ("No such media player \"%s\" found in \"%s\"",
               G_OBJECT_TYPE_NAME (player),
               G_OBJECT_TYPE_NAME (adapter));
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_media_adapter_dispose (GObject *object)
{
  ValentMediaAdapter *self = VALENT_MEDIA_ADAPTER (object);
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (self);

  g_clear_pointer (&priv->players, g_ptr_array_unref);

  G_OBJECT_CLASS (valent_media_adapter_parent_class)->dispose (object);
}

static void
valent_media_adapter_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ValentMediaAdapter *self = VALENT_MEDIA_ADAPTER (object);
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (self);

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
valent_media_adapter_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ValentMediaAdapter *self = VALENT_MEDIA_ADAPTER (object);
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (self);

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
valent_media_adapter_class_init (ValentMediaAdapterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_media_adapter_dispose;
  object_class->get_property = valent_media_adapter_get_property;
  object_class->set_property = valent_media_adapter_set_property;

  klass->player_added = valent_media_adapter_real_player_added;
  klass->player_removed = valent_media_adapter_real_player_removed;
  klass->export_player = valent_media_adapter_real_export_player;
  klass->unexport_player = valent_media_adapter_real_unexport_player;

  /**
   * ValentMediaAdapter:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this adapter.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info", NULL, NULL,
                        PEAS_TYPE_PLUGIN_INFO,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentMediaAdapter::player-added:
   * @adapter: an #ValentMediaAdapter
   * @player: an #ValentMediaPlayer
   *
   * Emitted when a [class@Valent.MediaPlayer] has been added to @adapter.
   *
   * Implementations of #ValentMediaAdapter must chain-up if they
   * override [vfunc@Valent.MediaAdapter.player_added].
   *
   * Since: 1.0
   */
  signals [PLAYER_ADDED] =
    g_signal_new ("player-added",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentMediaAdapterClass, player_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MEDIA_PLAYER);
  g_signal_set_va_marshaller (signals [PLAYER_ADDED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);

  /**
   * ValentMediaAdapter::player-removed:
   * @adapter: an #ValentMediaAdapter
   * @player: an #ValentMediaPlayer
   *
   * Emitted when a [class@Valent.MediaPlayer] has been removed from @adapter.
   *
   * Implementations of #ValentMediaAdapter must chain-up if they
   * override [vfunc@Valent.MediaAdapter.player_removed].
   *
   * Since: 1.0
   */
  signals [PLAYER_REMOVED] =
    g_signal_new ("player-removed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ValentMediaAdapterClass, player_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1, VALENT_TYPE_MEDIA_PLAYER);
  g_signal_set_va_marshaller (signals [PLAYER_REMOVED],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__OBJECTv);
}

static void
valent_media_adapter_init (ValentMediaAdapter *adapter)
{
}

/**
 * valent_media_adapter_player_added:
 * @adapter: a #ValentMediaAdapter
 * @player: a #ValentMediaPlayer
 *
 * Emit [signal@Valent.MediaAdapter::player-added] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.MediaAdapter].
 *
 * Since: 1.0
 */
void
valent_media_adapter_player_added (ValentMediaAdapter *adapter,
                                   ValentMediaPlayer  *player)
{
  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_signal_emit (G_OBJECT (adapter), signals [PLAYER_ADDED], 0, player);
}

/**
 * valent_media_adapter_player_removed:
 * @adapter: a #ValentMediaAdapter
 * @player: a #ValentMediaPlayer
 *
 * Emit [signal@Valent.MediaAdapter::player-removed] on @adapter.
 *
 * This method should only be called by implementations of
 * [class@Valent.MediaAdapter].
 *
 * Since: 1.0
 */
void
valent_media_adapter_player_removed (ValentMediaAdapter *adapter,
                                     ValentMediaPlayer  *player)
{
  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_object_ref (player);
  g_signal_emit (G_OBJECT (adapter), signals [PLAYER_REMOVED], 0, player);
  g_object_unref (player);
}

/**
 * valent_media_adapter_export_player: (virtual export_player)
 * @adapter: an #ValentMediaAdapter
 * @player: a #ValentMediaPlayer
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
 * @adapter: an #ValentMediaAdapter
 * @player: a #ValentMediaPlayer
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

/**
 * valent_media_adapter_get_players:
 * @adapter: an #ValentMediaAdapter
 *
 * Gets a new #GPtrArray containing a list of #ValentMediaPlayer instances that
 * were registered by @adapter.
 *
 * Returns: (transfer container) (element-type Valent.MediaPlayer): a list of
 *   players.
 *
 * Since: 1.0
 */
GPtrArray *
valent_media_adapter_get_players (ValentMediaAdapter *adapter)
{
  ValentMediaAdapterPrivate *priv = valent_media_adapter_get_instance_private (adapter);
  GPtrArray *ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter), NULL);

  ret = g_ptr_array_new_with_free_func (g_object_unref);

  if (priv->players != NULL)
    {
      for (unsigned int i = 0; i < priv->players->len; i++)
        g_ptr_array_add (ret, g_object_ref (g_ptr_array_index (priv->players, i)));
    }

  VALENT_RETURN (ret);
}

