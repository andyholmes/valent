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
 * @load_async: the virtual function pointer for valent_media_adapter_load_async()
 * @load_finish: the virtual function pointer for valent_media_adapter_load_finish()
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
valent_media_adapter_real_load_async (ValentMediaAdapter  *adapter,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_task_report_new_error (adapter, callback, user_data,
                           valent_media_adapter_real_load_async,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement load_async",
                           G_OBJECT_TYPE_NAME (adapter));
}

static gboolean
valent_media_adapter_real_load_finish (ValentMediaAdapter  *adapter,
                                       GAsyncResult        *result,
                                       GError             **error)
{
  g_assert (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_assert (g_task_is_valid (result, adapter));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
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
  klass->load_async = valent_media_adapter_real_load_async;
  klass->load_finish = valent_media_adapter_real_load_finish;

  /**
   * ValentMediaAdapter:plugin-info:
   *
   * The [struct@Peas.PluginInfo] describing this adapter.
   *
   * Since: 1.0
   */
  properties [PROP_PLUGIN_INFO] =
    g_param_spec_boxed ("plugin-info",
                        "Plugin Info",
                        "The plugin info describing this adapter",
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
 * valent_media_adapter_emit_player_added:
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
valent_media_adapter_emit_player_added (ValentMediaAdapter *adapter,
                                        ValentMediaPlayer  *player)
{
  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_signal_emit (G_OBJECT (adapter), signals [PLAYER_ADDED], 0, player);
}

/**
 * valent_media_adapter_emit_player_removed:
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
valent_media_adapter_emit_player_removed (ValentMediaAdapter *adapter,
                                          ValentMediaPlayer  *player)
{
  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (VALENT_IS_MEDIA_PLAYER (player));

  g_object_ref (player);
  g_signal_emit (G_OBJECT (adapter), signals [PLAYER_REMOVED], 0, player);
  g_object_unref (player);
}

/**
 * valent_media_adapter_load_async: (virtual load_async)
 * @adapter: an #ValentMediaAdapter
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user supplied data
 *
 * Load any media players known to @adapter.
 *
 * Implementations are expected to emit
 * [signal@Valent.MediaAdapter::player-added] for each player before
 * completing the operation.
 *
 * This method is called by the [class@Valent.Media] singleton and must only
 * be called once for each implementation. It is therefore a programmer error
 * for an API user to call this method.
 *
 * Since: 1.0
 */
void
valent_media_adapter_load_async (ValentMediaAdapter  *adapter,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  VALENT_MEDIA_ADAPTER_GET_CLASS (adapter)->load_async (adapter,
                                                        cancellable,
                                                        callback,
                                                        user_data);

  VALENT_EXIT;
}

/**
 * valent_media_adapter_load_finish: (virtual load_finish)
 * @adapter: an #ValentMediaAdapter
 * @result: a #GAsyncResult provided to callback
 * @error: (nullable): a #GError
 *
 * Finish an operation started by [method@Valent.MediaAdapter.load_async].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_media_adapter_load_finish (ValentMediaAdapter  *adapter,
                                  GAsyncResult        *result,
                                  GError             **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_MEDIA_ADAPTER (adapter), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, adapter), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_MEDIA_ADAPTER_GET_CLASS (adapter)->load_finish (adapter,
                                                               result,
                                                               error);

  VALENT_RETURN (ret);
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

