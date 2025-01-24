// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-transfer"

#include "config.h"

#include <gio/gio.h>

#include "valent-core-enums.h"
#include "valent-debug.h"
#include "valent-macros.h"
#include "valent-object.h"
#include "valent-transfer.h"


/**
 * ValentTransfer:
 *
 * An abstract base class for data transfers.
 *
 * `ValentTransfer` is a generic class for transfers.
 *
 * Since: 1.0
 */

typedef struct
{
  GError              *error;
  char                *id;
  double               progress;
  ValentTransferState  state;
} ValentTransferPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ValentTransfer, valent_transfer, VALENT_TYPE_OBJECT)

typedef enum {
  PROP_ID = 1,
  PROP_PROGRESS,
  PROP_STATE,
} ValentTransferProperty;

static GParamSpec *properties[PROP_STATE + 1] = { NULL, };


/* LCOV_EXCL_START */
static void
valent_transfer_real_execute (ValentTransfer      *transfer,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  g_assert (VALENT_IS_TRANSFER (transfer));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_task_report_new_error (transfer, callback, user_data,
                           valent_transfer_real_execute,
                           G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "%s does not implement execute()",
                           G_OBJECT_TYPE_NAME (transfer));
}

static gboolean
valent_transfer_real_execute_finish (ValentTransfer  *transfer,
                                     GAsyncResult    *result,
                                     GError         **error)
{
  g_assert (VALENT_IS_TRANSFER (transfer));
  g_assert (g_task_is_valid (result, transfer));
  g_assert (error == NULL || *error == NULL);

  return g_task_propagate_boolean (G_TASK (result), error);
}
/* LCOV_EXCL_STOP */

/*
 * GObject
 */
static void
valent_transfer_finalize (GObject *object)
{
  ValentTransfer *self = VALENT_TRANSFER (object);
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);

  valent_object_lock (VALENT_OBJECT (self));
  g_clear_error (&priv->error);
  g_clear_pointer (&priv->id, g_free);
  valent_object_unlock (VALENT_OBJECT (self));

  G_OBJECT_CLASS (valent_transfer_parent_class)->finalize (object);
}

static void
valent_transfer_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ValentTransfer *self = VALENT_TRANSFER (object);

  switch ((ValentTransferProperty)prop_id)
    {
    case PROP_ID:
      g_value_take_string (value, valent_transfer_dup_id (self));
      break;

    case PROP_PROGRESS:
      g_value_set_double (value, valent_transfer_get_progress (self));
      break;

    case PROP_STATE:
      g_value_set_enum (value, valent_transfer_get_state (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_transfer_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  ValentTransfer *self = VALENT_TRANSFER (object);
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);

  switch ((ValentTransferProperty)prop_id)
    {
    case PROP_ID:
      valent_object_lock (VALENT_OBJECT (self));
      priv->id = g_value_dup_string (value);
      valent_object_unlock (VALENT_OBJECT (self));
      break;

    case PROP_PROGRESS:
      valent_transfer_set_progress (self, g_value_get_double (value));
      break;

    case PROP_STATE:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_transfer_class_init (ValentTransferClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = valent_transfer_finalize;
  object_class->get_property = valent_transfer_get_property;
  object_class->set_property = valent_transfer_set_property;

  klass->execute = valent_transfer_real_execute;
  klass->execute_finish = valent_transfer_real_execute_finish;

  /**
   * ValentTransfer:id: (getter ref_id)
   *
   * A unique identifier for the transfer.
   *
   * If not specified at construction, a random UUID will be generated on-demand
   * with [func@GLib.uuid_string_random].
   *
   * This property is thread-safe. Emissions of [signal@GObject.Object::notify]
   * are guaranteed to happen in the main thread.
   *
   * Since: 1.0
   */
  properties [PROP_ID] =
    g_param_spec_string ("id", NULL, NULL,
                         NULL,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentTransfer:progress: (getter get_progress) (setter set_progress)
   *
   * The progress of the transfer.
   *
   * This value will change from `0.0` to `1.0` during the course of the
   * operation. It is guaranteed to change to `1.0` when the transfer operation
   * completes, but not guaranteed to change before that unless set by an
   * implementation.
   *
   * This property is thread-safe. Emissions of [signal@GObject.Object::notify]
   * are guaranteed to happen in the main thread.
   *
   * Since: 1.0
   */
  properties [PROP_PROGRESS] =
    g_param_spec_double ("progress", NULL, NULL,
                         0.0, 1.0,
                         0.0,
                         (G_PARAM_READWRITE |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  /**
   * ValentTransfer:state: (getter get_state)
   *
   * The [enum@Valent.TransferState] of the transfer.
   *
   * The value will change from %VALENT_TRANSFER_STATE_PENDING to
   * %VALENT_TRANSFER_STATE_ACTIVE when [method@Valent.Transfer.execute] is
   * called. When the operation completes it will change to either
   * %VALENT_TRANSFER_STATE_COMPLETE or %VALENT_TRANSFER_STATE_FAILED.
   *
   * This property is thread-safe. Emissions of [signal@GObject.Object::notify]
   * are guaranteed to happen in the main thread.
   *
   * Since: 1.0
   */
  properties [PROP_STATE] =
    g_param_spec_enum ("state", NULL, NULL,
                       VALENT_TYPE_TRANSFER_STATE,
                       VALENT_TRANSFER_STATE_PENDING,
                       (G_PARAM_READABLE |
                        G_PARAM_EXPLICIT_NOTIFY |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, G_N_ELEMENTS (properties), properties);
}

static void
valent_transfer_init (ValentTransfer *self)
{
}

/**
 * valent_transfer_dup_id: (get-property id)
 * @transfer: a `ValentTransfer`
 *
 * Get the transfer ID.
 *
 * Returns: (transfer full) (not nullable): a unique ID
 *
 * Since: 1.0
 */
char *
valent_transfer_dup_id (ValentTransfer *transfer)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  g_autofree char *ret = NULL;

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), NULL);

  valent_object_lock (VALENT_OBJECT (transfer));
  if (priv->id == NULL)
    priv->id = g_uuid_string_random ();
  ret = g_strdup (priv->id);
  valent_object_unlock (VALENT_OBJECT (transfer));

  return g_steal_pointer (&ret);
}

/**
 * valent_transfer_get_progress: (get-property progress)
 * @transfer: a `ValentTransfer`
 *
 * Get the transfer progress.
 *
 * Returns: a number from `0.0` to `1.0`
 *
 * Since: 1.0
 */
double
valent_transfer_get_progress (ValentTransfer *transfer)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  double ret;

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), 0.0);

  valent_object_lock (VALENT_OBJECT (transfer));
  ret = priv->progress;
  valent_object_unlock (VALENT_OBJECT (transfer));

  return ret;
}

/**
 * valent_transfer_set_progress: (set-property progress)
 * @transfer: a `ValentTransfer`
 * @progress: a number from `0.0` to `1.0`
 *
 * Set the transfer progress.
 *
 * This method should only be called by implementations of
 * [class@Valent.Transfer].
 *
 * Since: 1.0
 */
void
valent_transfer_set_progress (ValentTransfer *transfer,
                              double          progress)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (progress >= 0.0 && progress <= 1.0);

  valent_object_lock (VALENT_OBJECT (transfer));
  if (!G_APPROX_VALUE (priv->progress, progress, 0.01))
    {
      priv->progress = progress;
      valent_object_notify_by_pspec (VALENT_OBJECT (transfer),
                                     properties [PROP_PROGRESS]);
    }
  valent_object_unlock (VALENT_OBJECT (transfer));
}

/**
 * valent_transfer_get_state: (get-property state)
 * @transfer: a `ValentTransfer`
 *
 * Get the transfer state.
 *
 * Returns: a `ValentTransferState`
 *
 * Since: 1.0
 */
ValentTransferState
valent_transfer_get_state (ValentTransfer *transfer)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  ValentTransferState ret = VALENT_TRANSFER_STATE_PENDING;

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), FALSE);

  valent_object_lock (VALENT_OBJECT (transfer));
  ret = priv->state;
  valent_object_unlock (VALENT_OBJECT (transfer));

  return ret;
}

static void
valent_transfer_execute_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  ValentTransfer *self = VALENT_TRANSFER (object);
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (self);
  g_autoptr (GTask) task = G_TASK (user_data);

  VALENT_ENTRY;

  g_assert (VALENT_IS_TRANSFER (self));
  g_assert (g_task_is_valid (result, self));
  g_assert (G_IS_TASK (task));

  valent_transfer_set_progress (self, 1.0);

  valent_object_lock (VALENT_OBJECT (self));
  if (g_task_propagate_boolean (G_TASK (result), &priv->error))
    {
      priv->state = VALENT_TRANSFER_STATE_COMPLETE;
      valent_object_unlock (VALENT_OBJECT (self));

      g_task_return_boolean (task, TRUE);
    }
  else
    {
      priv->state = VALENT_TRANSFER_STATE_FAILED;
      valent_object_unlock (VALENT_OBJECT (self));

      g_task_return_error (task, g_error_copy (priv->error));
    }

  valent_object_notify_by_pspec (VALENT_OBJECT (self), properties [PROP_STATE]);

  VALENT_EXIT;
}

/**
 * valent_transfer_execute: (virtual execute)
 * @transfer: a `ValentTransfer`
 * @cancellable: (nullable): a `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Start the transfer operation.
 *
 * Get the result with [method@Valent.Transfer.execute_finish].
 *
 * If the transfer operation has already started, this call will fail and
 * [method@Valent.Transfer.execute_finish] will return %G_IO_ERROR_PENDING.
 *
 * Since: 1.0
 */
void
valent_transfer_execute (ValentTransfer      *transfer,
                         GCancellable        *cancellable,
                         GAsyncReadyCallback  callback,
                         gpointer             user_data)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  g_autoptr (GTask) task = NULL;
  g_autoptr (GCancellable) destroy = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  valent_object_lock (VALENT_OBJECT (transfer));
  if (priv->state != VALENT_TRANSFER_STATE_PENDING)
    {
      g_task_report_new_error (transfer, callback, user_data,
                               valent_transfer_execute,
                               G_IO_ERROR,
                               G_IO_ERROR_PENDING,
                               "%s is already in progress",
                               G_OBJECT_TYPE_NAME (transfer));
      valent_object_unlock (VALENT_OBJECT (transfer));
      VALENT_EXIT;
    }

  destroy = valent_object_chain_cancellable (VALENT_OBJECT (transfer),
                                             cancellable);

  task = g_task_new (transfer, destroy, callback, user_data);
  g_task_set_source_tag (task, valent_transfer_execute);

  VALENT_TRANSFER_GET_CLASS (transfer)->execute (transfer,
                                                 destroy,
                                                 valent_transfer_execute_cb,
                                                 g_steal_pointer (&task));

  priv->state = VALENT_TRANSFER_STATE_ACTIVE;
  valent_object_unlock (VALENT_OBJECT (transfer));

  valent_object_notify_by_pspec (VALENT_OBJECT (transfer), properties [PROP_STATE]);

  VALENT_EXIT;
}

/**
 * valent_transfer_execute_finish: (virtual execute_finish)
 * @transfer: a `ValentTransfer`
 * @result: a `GAsyncResult`
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [method@Valent.Transfer.execute].
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_transfer_execute_finish (ValentTransfer  *transfer,
                                GAsyncResult    *result,
                                GError         **error)
{
  gboolean ret;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, transfer), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = VALENT_TRANSFER_GET_CLASS (transfer)->execute_finish (transfer,
                                                              result,
                                                              error);

  VALENT_RETURN (ret);
}

/**
 * valent_transfer_cancel:
 * @transfer: a `ValentTransfer`
 *
 * Cancel the transfer operation.
 *
 * If this is called before [method@Valent.Transfer.execute] the transfer will
 * fail unconditionally.
 *
 * Since: 1.0
 */
void
valent_transfer_cancel (ValentTransfer *transfer)
{
  g_autoptr (GCancellable) cancellable = NULL;

  VALENT_ENTRY;

  g_return_if_fail (VALENT_IS_TRANSFER (transfer));

  cancellable = valent_object_ref_cancellable (VALENT_OBJECT (transfer));
  g_cancellable_cancel (cancellable);

  VALENT_EXIT;
}

/**
 * valent_transfer_check_status:
 * @transfer: a `ValentTransfer`
 * @error: (nullable): a `GError`
 *
 * Check the transfer status.
 *
 * Returns %TRUE if the transfer operation is in progress or completed
 * successfully. Returns %FALSE with @error set if the transfer failed.
 *
 * Returns: %TRUE, or %FALSE with @error set
 *
 * Since: 1.0
 */
gboolean
valent_transfer_check_status (ValentTransfer  *transfer,
                              GError         **error)
{
  ValentTransferPrivate *priv = valent_transfer_get_instance_private (transfer);
  gboolean ret = TRUE;

  VALENT_ENTRY;

  g_return_val_if_fail (VALENT_IS_TRANSFER (transfer), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  valent_object_lock (VALENT_OBJECT (transfer));
  if (priv->error != NULL)
    {
      if (error != NULL)
        *error = g_error_copy (priv->error);
      ret = FALSE;
    }
  valent_object_unlock (VALENT_OBJECT (transfer));

  VALENT_RETURN (ret);
}

