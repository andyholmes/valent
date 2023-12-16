// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-object"

#include "config.h"

#include "valent-macros.h"
#include "valent-object.h"

/**
 * ValentObject:
 *
 * A base class for objects.
 *
 * `ValentObject` is a specialized `GObject` class, based on GNOME Builder's
 * `IdeObject`. It provides a simple base class with helpers for working in
 * threads.
 *
 * Instances have a `GRecMutex` and a `GCancellable` that is created on demand by
 * valent_object_ref_cancellable() is called. When the object is destroyed, the
 * `GCancellable`::cancel signal is emitted.
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


static GQueue finalizer_queue = G_QUEUE_INIT;
static GMutex finalizer_mutex;
static GSource *finalizer_source;

static gboolean
valent_object_finalizer_source_check (GSource *source)
{
  return finalizer_queue.length > 0;
}

static gboolean
valent_object_finalizer_source_dispatch (GSource     *source,
                                         GSourceFunc  callback,
                                         gpointer     user_data)
{
  while (finalizer_queue.length)
    {
      g_autoptr (GObject) object = g_queue_pop_head (&finalizer_queue);
      g_object_run_dispose (object);
    }

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs finalizer_source_funcs = {
  .check = valent_object_finalizer_source_check,
  .dispatch = valent_object_finalizer_source_dispatch,
};


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

/*
 * GObject.Object::notify
 */
typedef struct
{
  GRecMutex   mutex;
  GWeakRef    object;
  GParamSpec *pspec;
  char       *property_name;
} NotifyEmission;

static gboolean
valent_object_notify_main (gpointer data)
{
  NotifyEmission *emission = data;
  g_autoptr (GObject) object = NULL;

  g_assert (emission != NULL);

  g_rec_mutex_lock (&emission->mutex);
  if ((object = g_weak_ref_get (&emission->object)) != NULL)
    {
      if (emission->pspec != NULL)
        g_object_notify_by_pspec (object, emission->pspec);
      else
        g_object_notify (object, emission->property_name);
    }

  g_weak_ref_clear (&emission->object);
  g_clear_pointer (&emission->property_name, g_free);
  g_clear_pointer (&emission->pspec, g_param_spec_unref);
  g_rec_mutex_unlock (&emission->mutex);
  g_rec_mutex_clear (&emission->mutex);
  g_clear_pointer (&emission, g_free);

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
      g_mutex_lock (&finalizer_mutex);
      g_queue_push_tail (&finalizer_queue, g_object_ref (self));
      g_mutex_unlock (&finalizer_mutex);
      g_main_context_wakeup (NULL);
      return;
    }

  g_assert (VALENT_IS_OBJECT (object));
  g_assert (VALENT_IS_MAIN_THREAD ());

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
valent_object_constructed (GObject *object)
{
  if G_UNLIKELY (G_OBJECT_GET_CLASS (object)->dispose != valent_object_dispose)
    g_error ("%s overrides GObject.Object.dispose() instead of "
             "Valent.Object.destroy(), which is not thread safe",
             G_OBJECT_TYPE_NAME (object));

  G_OBJECT_CLASS (valent_object_parent_class)->constructed (object);
}

static void
valent_object_finalize (GObject *object)
{
  ValentObject *self = VALENT_OBJECT (object);
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

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

  object_class->constructed = valent_object_constructed;
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
   * A `GCancellable` that can be used by operations that should be cancelled
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
   * This signal is emitted when the object enters disposal and always on the
   * main thread, with the object lock acquired. Note that you must still drop
   * any references you hold to avoid leaking memory.
   *
   * Implementations must override [vfunc@Valent.Object.destroy] instead of
   * [vfunc@GObject.Object.dispose] to ensure the instance is finalized on the
   * main thread.
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

  /* Setup the finalizer in main thread to receive off-thread objects */
  finalizer_source = g_source_new (&finalizer_source_funcs, sizeof (GSource));
  g_source_set_static_name (finalizer_source, "[valent-object-finalizer]");
  g_source_set_priority (finalizer_source, G_MAXINT);
  g_source_attach (finalizer_source, NULL);
}

static void
valent_object_init (ValentObject *self)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

  g_rec_mutex_init (&priv->mutex);
}

/**
 * valent_object_lock:
 * @object: a `ValentObject`
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
 * @object: a `ValentObject`
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
 * @object: a `ValentObject`
 *
 * Get a [class@Gio.Cancellable] for the object.
 *
 * Returns: (transfer full) (not nullable): @object's `GCancellable`
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
 * valent_object_chain_cancellable:
 * @object: a `ValentObject`
 * @cancellable: (nullable): a `GCancellable`
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
 * Returns: (transfer full) (not nullable): a `GCancellable`
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
 * @object: a `ValentObject`
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
      g_mutex_lock (&finalizer_mutex);
      g_queue_push_tail (&finalizer_queue, g_object_ref (object));
      g_mutex_unlock (&finalizer_mutex);
    }

  valent_object_private_unlock (priv);
  g_object_unref (object);
}

/**
 * valent_object_in_destruction:
 * @object: a `ValentObject`
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

/**
 * valent_object_notify:
 * @object: a `ValentObject`
 * @property_name: a property name
 *
 * Emit [signal@GObject.Object::notify] on @object, on the main thread.
 *
 * Like [method@GObject.Object.notify] if the caller is in the main thread,
 * otherwise the invocation is deferred to the main thread.
 *
 * Since: 1.0
 */
void
valent_object_notify (ValentObject *object,
                      const char   *property_name)
{
  NotifyEmission *emission = NULL;

  g_return_if_fail (VALENT_IS_OBJECT (object));
  g_return_if_fail (property_name != NULL);

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_object_notify (G_OBJECT (object), property_name);
      return;
    }

  emission = g_new0 (NotifyEmission, 1);
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->object, object);
  emission->property_name = g_strdup (property_name);
  g_rec_mutex_unlock (&emission->mutex);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   valent_object_notify_main,
                   g_steal_pointer (&emission),
                   NULL);
}

/**
 * valent_object_notify_by_pspec:
 * @object: a `ValentObject`
 * @pspec: a `GParamSpec`
 *
 * Emit [signal@GObject.Object::notify] on @object, on the main thread.
 *
 * Like [method@GObject.Object.notify_by_pspec] if the caller is in the main
 * thread, otherwise the invocation is deferred to the main thread.
 *
 * Since: 1.0
 */
void
valent_object_notify_by_pspec (ValentObject *object,
                               GParamSpec   *pspec)
{
  NotifyEmission *emission = NULL;

  g_return_if_fail (VALENT_IS_OBJECT (object));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  if G_LIKELY (VALENT_IS_MAIN_THREAD ())
    {
      g_object_notify_by_pspec (G_OBJECT (object), pspec);
      return;
    }

  emission = g_new0 (NotifyEmission, 1);
  g_rec_mutex_init (&emission->mutex);
  g_rec_mutex_lock (&emission->mutex);
  g_weak_ref_init (&emission->object, object);
  emission->pspec = g_param_spec_ref (pspec);
  g_rec_mutex_unlock (&emission->mutex);

  g_idle_add_full (G_PRIORITY_DEFAULT,
                   valent_object_notify_main,
                   g_steal_pointer (&emission),
                   NULL);
}
