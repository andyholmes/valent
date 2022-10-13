// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-object"

#include "config.h"

#include "valent-object.h"
#include "valent-macros.h"

/**
 * ValentObject:
 *
 * A base class for objects.
 *
 * #ValentObject is a specialized #GObject class, based on GNOME Builder's
 * `IdeObject`. It provides a simple base class with helpers for working in
 * threads.
 *
 * Instances have a #GRecMutex and a #GCancellable that is created on demand by
 * valent_object_ref_cancellable() is called. When the object is destroyed, the
 * #GCancellable::cancel signal is emitted.
 *
 * Since: 1.0
 */

typedef struct
{
  GRecMutex     mutex;
  GCancellable *cancellable;
  unsigned int  in_destruction : 1;
  unsigned int  destroyed : 1;
} ValentObjectPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (ValentObject, valent_object, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_CANCELLABLE,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
  DESTROY,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0, };


static inline void
valent_object_private_lock (ValentObjectPrivate *priv)
{
  g_rec_mutex_lock (&priv->mutex);
}

static inline void
valent_object_private_unlock (ValentObjectPrivate *priv)
{
  g_rec_mutex_unlock (&priv->mutex);
}

static gboolean
valent_object_destroy_main (ValentObject *object)
{
  g_assert (VALENT_IS_MAIN_THREAD ());
  g_assert (VALENT_IS_OBJECT (object));

  valent_object_destroy (object);

  return G_SOURCE_REMOVE;
}

static gboolean
valent_object_dispose_main (ValentObject *object)
{
  g_assert (VALENT_IS_MAIN_THREAD ());
  g_assert (VALENT_IS_OBJECT (object));

  g_object_run_dispose (G_OBJECT (object));

  return G_SOURCE_REMOVE;
}

/*
 * ValentObject
 */
static void
valent_object_real_destroy (ValentObject *self)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

  g_assert (VALENT_IS_OBJECT (self));

  g_cancellable_cancel (priv->cancellable);
  priv->destroyed = TRUE;
}

/*
 * GObject
 */
static void
valent_object_dispose (GObject *object)
{
  ValentObject *self = VALENT_OBJECT (object);
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

  if (!VALENT_IS_MAIN_THREAD ())
    {
      /* We are not on the main thread and might lose our last reference count.
       * Pass this object to the main thread for disposal. This usually only
       * happens when an object was temporarily created/destroyed on a thread.
       */
      g_idle_add_full (G_PRIORITY_LOW + 1000,
                       (GSourceFunc)valent_object_dispose_main,
                       g_object_ref (self),
                       g_object_unref);
      return;
    }

  g_assert (VALENT_IS_OBJECT (object));

  valent_object_private_lock (priv);
  if (!priv->in_destruction)
    {
      priv->in_destruction = TRUE;
      g_signal_emit (self, signals [DESTROY], 0);
      priv->in_destruction = FALSE;
    }
  valent_object_private_unlock (priv);

  G_OBJECT_CLASS (valent_object_parent_class)->dispose (object);
}

static void
valent_object_finalize (GObject *object)
{
  ValentObject *self = VALENT_OBJECT (object);
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

  if (!VALENT_IS_MAIN_THREAD ())
    {
      g_critical ("%s: attempt to finalize off the main thread; leaking instead",
                  G_OBJECT_TYPE_NAME (object));
      return;
    }

  g_clear_object (&priv->cancellable);
  g_rec_mutex_clear (&priv->mutex);

  G_OBJECT_CLASS (valent_object_parent_class)->finalize (object);
}

static void
valent_object_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  ValentObject *self = VALENT_OBJECT (object);

  switch (prop_id)
    {
    case PROP_CANCELLABLE:
      g_value_take_object (value, valent_object_ref_cancellable (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_object_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  ValentObject *self = VALENT_OBJECT (object);
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_CANCELLABLE:
      priv->cancellable = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_object_class_init (ValentObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_object_dispose;
  object_class->finalize = valent_object_finalize;
  object_class->get_property = valent_object_get_property;
  object_class->set_property = valent_object_set_property;

  klass->destroy = valent_object_real_destroy;

  /**
   * ValentObject:cancellable: (getter ref_cancellable)
   *
   * The object [class@Gio.Cancellable].
   *
   * A #GCancellable that can be used by operations that should be cancelled
   * when the object is destroyed (i.e. enters disposal).
   *
   * Since: 1.0
   */
  properties [PROP_CANCELLABLE] =
    g_param_spec_object ("cancellable", NULL, NULL,
                         G_TYPE_CANCELLABLE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentObject::destroy:
   *
   * Emitted when the object is being destroyed.
   *
   * This signal is emitted when the object enter disposal and always on the
   * main thread. Note that you must still drop any references you hold to the
   * object to avoid leaking memory.
   *
   * Implementations that override [vfunc@Valent.Object.destroy] must chain-up.
   *
   * Since: 1.0
   */
  signals [DESTROY] =
    g_signal_new ("destroy",
                  G_TYPE_FROM_CLASS (klass),
                  (G_SIGNAL_RUN_CLEANUP |
                   G_SIGNAL_NO_RECURSE |
                   G_SIGNAL_NO_HOOKS),
                  G_STRUCT_OFFSET (ValentObjectClass, destroy),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  g_signal_set_va_marshaller (signals [DESTROY],
                              G_TYPE_FROM_CLASS (klass),
                              g_cclosure_marshal_VOID__VOIDv);
}

static void
valent_object_init (ValentObject *self)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

  g_rec_mutex_init (&priv->mutex);
}

/**
 * valent_object_lock:
 * @object: a #ValentObject
 *
 * Acquire a lock on @object.
 *
 * Call [method@Valent.Object.unlock] to release the lock.
 *
 * Since: 1.0
 */
void
valent_object_lock (ValentObject *object)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (object);

  g_return_if_fail (VALENT_IS_OBJECT (object));

  valent_object_private_lock (priv);
}

/**
 * valent_object_unlock:
 * @object: a #ValentObject
 *
 * Release a lock on @object.
 *
 * The lock must have previously been acquired by [method@Valent.Object.lock].
 *
 * Since: 1.0
 */
void
valent_object_unlock (ValentObject *object)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (object);

  g_return_if_fail (VALENT_IS_OBJECT (object));

  valent_object_private_unlock (priv);
}

/**
 * valent_object_ref_cancellable:
 * @object: a #ValentObject
 *
 * Get a [class@Gio.Cancellable] for the object.
 *
 * Returns: (transfer full) (not nullable): @object's #GCancellable
 *
 * Since: 1.0
 */
GCancellable *
valent_object_ref_cancellable (ValentObject *object)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (object);
  GCancellable *ret;

  g_return_val_if_fail (VALENT_IS_OBJECT (object), NULL);

  valent_object_private_lock (priv);
  if (priv->cancellable == NULL)
    priv->cancellable = g_cancellable_new ();
  ret = g_object_ref (priv->cancellable);
  valent_object_private_unlock (priv);

  return g_steal_pointer (&ret);
}

/**
 * valent_object_attach_cancellable:
 * @object: a #ValentObject
 * @cancellable: (nullable): a #GCancellable
 *
 * Attach the object's cancellable another cancellable.
 *
 * This connects [property@Valent.Object:cancellable] to @cancellable's
 * [signal@Gio.Cancellable::cancelled] so that if @cancellable is cancelled,
 * @object's cancellable will be cancelled. For convenience, this method returns
 * a reference of [property@Valent.Object:cancellable].
 *
 * Typically the returned [class@Gio.Cancellable] is passed to a
 * [iface@Gio.AsyncInitable] implementation to ensure initialization is
 * cancelled if @cancellable is triggered or @object is destroyed.
 *
 * ```c
 * static void
 * foo_async_initable_init_async (GAsyncInitable      *initable,
 *                                int                  io_priority,
 *                                GCancellable        *cancellable,
 *                                GAsyncReadyCallback  callback,
 *                                gpointer             user_data)
 * {
 *   g_autoptr (GTask) task = NULL;
 *   g_autoptr (GCancellable) destroy = NULL;
 *
 *   g_assert (VALENT_IS_OBJECT (initable));
 *
 *   destroy = valent_object_attach_cancellable (VALENT_OBJECT (initable),
 *                                               cancellable);
 *
 *   task = g_task_new (initable, destroy, callback, user_data);
 *   g_task_set_priority (task, io_priority);
 *   g_task_run_in_thread (task, foo_async_initable_init_task);
 * }
 * ```
 *
 * Returns: (transfer full) (not nullable): @object's #GCancellable
 *
 * Since: 1.0
 */
GCancellable *
valent_object_attach_cancellable (ValentObject *object,
                                  GCancellable *cancellable)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (object);
  GCancellable *ret;

  g_return_val_if_fail (VALENT_IS_OBJECT (object), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);

  valent_object_private_lock (priv);
  if (priv->cancellable == NULL)
    priv->cancellable = g_cancellable_new ();

  if (cancellable != NULL)
    g_signal_connect_object (cancellable,
                             "cancelled",
                             G_CALLBACK (g_cancellable_cancel),
                             priv->cancellable,
                             G_CONNECT_SWAPPED);

  ret = g_object_ref (priv->cancellable);
  valent_object_private_unlock (priv);

  return g_steal_pointer (&ret);
}

/**
 * valent_object_chain_cancellable:
 * @object: a #ValentObject
 * @cancellable: (nullable): a #GCancellable
 *
 * Chain a cancellable to the object's cancellable.
 *
 * This connects @cancellable to @objects's [signal@Gio.Cancellable::cancelled]
 * so that if @object is destroyed, @cancellable will be cancelled. If
 * @cancellable is %NULL, this method will return a new reference to
 * [property@Valent.Object:cancellable], otherwise it returns a new reference to
 * @cancellable.
 *
 * Typically the returned [class@Gio.Cancellable] is passed to an internal
 * asynchronous operation, to ensure it is cancelled if @cancellable is
 * triggered or @object is destroyed.
 *
 * Returns: (transfer full) (not nullable): a #GCancellable
 *
 * Since: 1.0
 */
GCancellable *
valent_object_chain_cancellable (ValentObject *object,
                                 GCancellable *cancellable)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (object);
  GCancellable *ret;

  g_return_val_if_fail (VALENT_IS_OBJECT (object), NULL);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), NULL);

  valent_object_private_lock (priv);
  if (priv->cancellable == NULL)
    priv->cancellable = g_cancellable_new ();

  if (cancellable != NULL)
    {
      g_signal_connect_object (priv->cancellable,
                               "cancelled",
                               G_CALLBACK (g_cancellable_cancel),
                               cancellable,
                               G_CONNECT_SWAPPED);
      ret = g_object_ref (cancellable);
    }
  else
    {
      ret = g_object_ref (priv->cancellable);
    }
  valent_object_private_unlock (priv);

  return g_steal_pointer (&ret);
}

/**
 * valent_object_destroy:
 * @object: a #ValentObject
 *
 * Destroy the object.
 *
 * If called from the main thread, it calls [method@GObject.Object.run_dispose],
 * which activates the object [class@Gio.Cancellable] and emits
 * [signal@Valent.Object::destroy].
 *
 * If called from another thread, an idle source will be added to invoke it on
 * the main thread.
 *
 * Since: 1.0
 */
void
valent_object_destroy (ValentObject *object)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (object);

  g_return_if_fail (VALENT_IS_OBJECT (object));

  g_object_ref (object);
  valent_object_private_lock (priv);

  if (VALENT_IS_MAIN_THREAD ())
    {
      g_cancellable_cancel (priv->cancellable);

      if (!priv->in_destruction && !priv->destroyed)
        g_object_run_dispose (G_OBJECT (object));
    }
  else
    {
      g_idle_add_full (G_PRIORITY_LOW + 1000,
                       (GSourceFunc)valent_object_destroy_main,
                       g_object_ref (object),
                       g_object_unref);
    }

  valent_object_private_unlock (priv);
  g_object_unref (object);
}

/**
 * valent_object_in_destruction:
 * @object: a #ValentObject
 *
 * Get whether the object is destroyed or in destruction.
 *
 * Returns: %TRUE if destroyed, or %FALSE if not
 *
 * Since: 1.0
 */
gboolean
valent_object_in_destruction (ValentObject *object)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (object);
  gboolean ret;

  g_return_val_if_fail (VALENT_IS_OBJECT (object), FALSE);

  valent_object_lock (object);
  ret = priv->in_destruction || priv->destroyed;
  valent_object_unlock (object);

  return ret;
}

