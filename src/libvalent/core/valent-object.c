// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014-2019 Christian Hergert <chergert@redhat.com>
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-object"

#include "config.h"

#include "valent-object.h"
#include "valent-macros.h"

/**
 * SECTION:valentobject
 * @title: ValentObject
 * @short_description: Base object with support for object trees
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * #ValentObject is a specialized #GObject class, based on GNOME Builder's
 * `IdeObject`. It provides a simple base class with helpers for working in
 * threads.
 *
 * Instances have a #GRecMutex and a #GCancellable that is created on demand by
 * valent_object_ref_cancellable() is called. When the object is destroyed, the
 * #GCancellable::cancel signal is emitted.
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
valent_object_dispose_main (gpointer data)
{
  ValentObject *self = VALENT_OBJECT (data);

  g_assert (VALENT_IS_OBJECT (data));

  g_object_run_dispose (G_OBJECT (self));

  return G_SOURCE_REMOVE;
}


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
                       valent_object_dispose_main,
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
      g_critical ("Attempt to finalize %s off the main thread; leaking instead",
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
valent_object_class_init (ValentObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_object_dispose;
  object_class->finalize = valent_object_finalize;
  object_class->get_property = valent_object_get_property;

  klass->destroy = valent_object_real_destroy;

  /**
   * ValentObject:cancellable:
   *
   * The "cancellable" property is a #GCancellable that can be used by
   * operations that will be cancelled when the #ValentObject::destroy signal is
   * emitted on @self.
   *
   * This is convenient when you want operations to automatically be cancelled
   * when part of the object tree is segmented.
   */
  properties [PROP_CANCELLABLE] =
    g_param_spec_object ("cancellable",
                         "Cancellable",
                         "A cancellable for the object",
                         G_TYPE_CANCELLABLE,
                         (G_PARAM_READABLE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);

  /**
   * ValentObject::destroy:
   *
   * The "destroy" signal is emitted when the object should destroy itself
   * and cleanup any state that is no longer necessary. This happens when
   * the object has been removed from the because it was requested to be
   * destroyed, or because a parent object is being destroyed.
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
 * Acquires a lock on @object. Call valent_object_unlock() to release the lock.
 *
 * The synchronization used is a #GRecMutex.
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
 * Releases a lock previously acquired with valent_object_lock().
 *
 * The synchronization used is a #GRecMutex.
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
 * Gets a #GCancellable for the object.
 *
 * Returns: (transfer full) (not nullable): a #GCancellable
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
 * valent_object_destroy:
 * @object: a #ValentObject
 *
 * Destroys @object.
 */
void
valent_object_destroy (ValentObject *self)
{
  ValentObjectPrivate *priv = valent_object_get_instance_private (self);

  g_return_if_fail (VALENT_IS_OBJECT (self));

  g_object_ref (self);
  valent_object_private_lock (priv);

  if (VALENT_IS_MAIN_THREAD ())
    {
      g_cancellable_cancel (priv->cancellable);

      if (!priv->in_destruction && !priv->destroyed)
        g_object_run_dispose (G_OBJECT (self));
    }
  else
    {
      g_idle_add_full (G_PRIORITY_LOW + 1000,
                       (GSourceFunc)valent_object_destroy_main,
                       g_object_ref (self),
                       g_object_unref);
    }

  valent_object_private_unlock (priv);
  g_object_unref (self);
}

/**
 * valent_object_in_destruction:
 * @object: a #ValentObject
 *
 * Checks if @object is destroyed or in destruction.
 *
 * Returns: %TRUE if destroyed, or %FALSE if not
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

