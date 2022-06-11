// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>
// SPDX-FileCopyrightText: Copyright 2015 Lars Uebernickel
// SPDX-FileCopyrightText: Copyright 2015 Ryan Lortie

#define G_LOG_DOMAIN "valent-sms-store"

#include "config.h"

#include <gio/gio.h>
#include <libvalent-core.h>

#include "valent-message-thread.h"
#include "valent-sms-store.h"
#include "valent-sms-store-private.h"


struct _ValentMessageThread
{
  GObject         parent_instance;

  ValentSmsStore *store;
  gint64          id;
  GCancellable   *cancellable;
  GSequence      *items;

  /* cache */
  unsigned int    last_position;
  GSequenceIter  *last_iter;
  gboolean        last_position_valid;
};

static void   g_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (ValentMessageThread, valent_message_thread, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_iface_init))

enum {
  PROP_0,
  PROP_STORE,
  PROP_ID,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };


static int
valent_message_thread_lookup_func (gconstpointer a,
                                   gconstpointer b,
                                   gpointer      user_data)
{
  gint *id = user_data;

  return valent_message_get_id ((ValentMessage *)a) == *id ? 0 : 1;
}

static int
valent_message_thread_sort_func (gconstpointer a,
                                 gconstpointer b,
                                 gpointer      user_data)
{
  gint64 date1 = valent_message_get_date ((ValentMessage *)a);
  gint64 date2 = valent_message_get_date ((ValentMessage *)b);

  return (date1 < date2) ? -1 : (date1 > date2);
}

static void
on_message_added (ValentSmsStore      *store,
                  ValentMessage       *message,
                  ValentMessageThread *self)
{
  // FIXME
  if (self->id == 0)
    {

    }

  if (self->id == valent_message_get_thread_id (message))
    {
      // TODO: search for a matching id, then insert
    }
}

static void
on_message_removed (ValentSmsStore      *store,
                    ValentMessage       *message,
                    ValentMessageThread *self)
{
  GSequenceIter *it;
  unsigned int position;
  gint64 id;

  if (self->id != valent_message_get_thread_id (message))
    return;

  id = valent_message_get_id (message);
  it = g_sequence_lookup (self->items,
                          &id,
                          valent_message_thread_lookup_func,
                          NULL);
  position = g_sequence_iter_get_position (it);

  g_list_model_items_changed (G_LIST_MODEL (self), position, 1, 0);
}

static unsigned int
valent_message_thread_add_message (ValentMessageThread *self,
                                   ValentMessage       *message)
{
  GSequenceIter *it;
  unsigned int position;

  g_assert (VALENT_IS_MESSAGE_THREAD (self));
  g_assert (VALENT_IS_MESSAGE (message));

  it = g_sequence_insert_sorted (self->items,
                                 g_object_ref (message),
                                 valent_message_thread_sort_func, NULL);
  position = g_sequence_iter_get_position (it);

  g_list_model_items_changed (G_LIST_MODEL (self), position, 0, 1);

  return position;
}

static void
get_message_cb (ValentSmsStore *store,
                GAsyncResult   *result,
                gpointer        user_data)
{
  g_autoptr (ValentMessage) message = VALENT_MESSAGE (user_data);
  g_autoptr (ValentMessage) update = NULL;
  g_autoptr (GError) error = NULL;

  update = valent_sms_store_get_message_finish (store, result, &error);

  if (update == NULL)
    g_warning ("%s: %s", G_STRFUNC, error->message);
  else
    valent_message_update (message, g_steal_pointer (&update));
}

static void
get_thread_items_cb (ValentSmsStore *store,
                     GAsyncResult   *result,
                     gpointer        user_data)
{
  g_autoptr (ValentMessageThread) self = VALENT_MESSAGE_THREAD (user_data);
  g_autoptr (GPtrArray) messages = NULL;
  g_autoptr (GError) error = NULL;
  unsigned int n_items;

  if ((messages = g_task_propagate_pointer (G_TASK (result), &error)) == NULL)
    {
      g_warning ("%s(): loading thread %"G_GINT64_FORMAT": %s",
                 G_STRFUNC,
                 self->id,
                 error->message);
      return;
    }

  /* Update the model */
  n_items = messages->len;

  for (unsigned int i = 0; i < n_items; i++)
    {
      ValentMessage *message;

      message = g_ptr_array_index (messages, i);
      g_sequence_append (self->items, g_object_ref (message));
    }

  g_list_model_items_changed (G_LIST_MODEL (self), 0, 0, n_items);
}


/*
 * GListModel
 */
static GType
valent_message_thread_get_item_type (GListModel *model)
{
  return VALENT_TYPE_MESSAGE;
}

static unsigned int
valent_message_thread_get_n_items (GListModel *model)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (model);

  return g_sequence_get_length (self->items);
}

static gpointer
valent_message_thread_get_item (GListModel   *model,
                                unsigned int  position)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (model);
  ValentMessage *message = NULL;
  GSequenceIter *it = NULL;

  if (self->last_position_valid)
    {
      if (position < G_MAXUINT && self->last_position == position + 1)
        it = g_sequence_iter_prev (self->last_iter);
      else if (position > 0 && self->last_position == position - 1)
        it = g_sequence_iter_next (self->last_iter);
      else if (self->last_position == position)
        it = self->last_iter;
    }

  if (it == NULL)
    it = g_sequence_get_iter_at_pos (self->items, position);

  self->last_iter = it;
  self->last_position = position;
  self->last_position_valid = TRUE;

  if (g_sequence_iter_is_end (it))
    return NULL;

  message = g_object_ref (g_sequence_get (it));

  /* Lazy fetch */
  if (valent_message_get_box (message) == 0)
    {
      valent_sms_store_get_message (self->store,
                                    valent_message_get_id (message),
                                    self->cancellable,
                                    (GAsyncReadyCallback)get_message_cb,
                                    g_object_ref (message));
    }

  return message;
}

static void
g_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item = valent_message_thread_get_item;
  iface->get_item_type = valent_message_thread_get_item_type;
  iface->get_n_items = valent_message_thread_get_n_items;
}


/*
 * GObject
 */
static void
valent_message_thread_dispose (GObject *object)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  g_cancellable_cancel (self->cancellable);

  G_OBJECT_CLASS (valent_message_thread_parent_class)->dispose (object);
}

static void
valent_message_thread_finalize (GObject *object)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  g_clear_object (&self->store);
  g_clear_pointer (&self->items, g_sequence_free);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (valent_message_thread_parent_class)->finalize (object);
}

static void
valent_message_thread_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_int64 (value, self->id);
      break;

    case PROP_STORE:
      g_value_set_object (value, self->store);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_thread_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ValentMessageThread *self = VALENT_MESSAGE_THREAD (object);

  switch (prop_id)
    {
    case PROP_ID:
      valent_message_thread_set_id (self, g_value_get_int64 (value));
      break;

    case PROP_STORE:
      self->store = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
valent_message_thread_class_init (ValentMessageThreadClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = valent_message_thread_dispose;
  object_class->finalize = valent_message_thread_finalize;
  object_class->get_property = valent_message_thread_get_property;
  object_class->set_property = valent_message_thread_set_property;

  /**
   * ValentMessageThread:id:
   *
   * The ID of the thread.
   */
  properties [PROP_ID] =
    g_param_spec_int64 ("id", NULL, NULL,
                        G_MININT64, G_MAXINT64,
                        0,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT |
                         G_PARAM_EXPLICIT_NOTIFY |
                         G_PARAM_STATIC_STRINGS));

  /**
   * ValentMessageThread:message-store:
   *
   * The #ValentSmsStore providing #ValentMessage objects for the thread.
   */
  properties [PROP_STORE] =
    g_param_spec_object ("store", NULL, NULL,
                         VALENT_TYPE_SMS_STORE,
                         (G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_EXPLICIT_NOTIFY |
                          G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
valent_message_thread_init (ValentMessageThread *self)
{
  self->cancellable = g_cancellable_new ();
  self->items = g_sequence_new (g_object_unref);
}

/**
 * valent_message_thread_new:
 * @store: a @ValentSmsStore
 * @id: a thread ID
 *
 * Create a new #ValentMessageThread.
 *
 * Returns: (transfer full): a #GListModel
 */
GListModel *
valent_message_thread_new (ValentSmsStore *store,
                           gint64          id)
{
  g_return_val_if_fail (VALENT_IS_SMS_STORE (store), NULL);
  g_return_val_if_fail (id >= 0, NULL);

  return g_object_new (VALENT_TYPE_MESSAGE_THREAD,
                       "id",    id,
                       "store", store,
                       NULL);
}

/**
 * valent_message_thread_get_id:
 * @thread: a #ValentMessageThread
 *
 * Get the thread ID for @thread.
 *
 * Returns: an ID
 */
gint64
valent_message_thread_get_id (ValentMessageThread *thread)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_THREAD (thread), 0);

  return thread->id;
}

/**
 * valent_message_thread_set_id:
 * @thread: a #ValentMessageThread
 * @id: a thread ID
 *
 * Set the thread ID for @thread to @id.
 */
void
valent_message_thread_set_id (ValentMessageThread *thread,
                              gint64               id)
{
  g_return_if_fail (VALENT_IS_MESSAGE_THREAD (thread));
  g_return_if_fail (id >= 0);

  if (thread->id == id)
    return;

  thread->id = id;
  g_object_notify_by_pspec (G_OBJECT (thread), properties [PROP_ID]);

  /* Load the thread items, while holding a reference to the thread */
  valent_sms_store_get_thread_items (thread->store,
                                     thread->id,
                                     thread->cancellable,
                                     (GAsyncReadyCallback)get_thread_items_cb,
                                     g_object_ref (thread));
}

/**
 * valent_message_thread_get_db:
 * @thread: a #ValentMessageThread
 *
 * Get the thread ID for @thread.
 *
 * Returns: a #ValentSmsStore
 */
ValentSmsStore *
valent_message_thread_get_store (ValentMessageThread *thread)
{
  g_return_val_if_fail (VALENT_IS_MESSAGE_THREAD (thread), NULL);

  return thread->store;
}

