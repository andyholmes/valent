// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-task-queue"

#include "config.h"

#include <gio/gio.h>

#include "valent-task-queue.h"


/**
 * SECTION: valenttaskqueue
 * @short_description: A task execution queue
 * @title: ValentTaskQueue
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * The #ValentTaskQueue class is an execution queue for #GTask operations.
 *
 * Each #ValentTaskQueue instance has a dedicated thread where tasks are
 * executed sequentially. Queued tasks are automatically sorted by priority, as
 * reported by g_task_get_priority().
 */

struct _ValentTaskQueue
{
  GAsyncQueue  *tasks;
  unsigned int  closed : 1;
};

G_DEFINE_BOXED_TYPE (ValentTaskQueue, valent_task_queue, valent_task_queue_ref, valent_task_queue_unref)


/**
 * ValentTaskMode:
 * @VALENT_TASK_NONE: a no-op task
 * @VALENT_TASK_CONCURRENT: a concurrent task
 * @VALENT_TASK_SEQUENTIAL: a sequential task
 * @VALENT_TASK_CRITICAL: a sequential task which must complete successfully
 * @VALENT_TASK_TERMINAL: a sequential task which terminates the queue
 *
 * Enumeration of task execution modes.
 */
typedef enum
{
  VALENT_TASK_NONE,
  VALENT_TASK_CONCURRENT,
  VALENT_TASK_SEQUENTIAL,
  VALENT_TASK_CRITICAL,
  VALENT_TASK_TERMINAL,
} ValentTaskMode;


/*
 * ValentTaskClosure
 */
typedef struct
{
  GTask           *task;
  GTaskThreadFunc  task_func;
  ValentTaskMode   task_mode;
  int              priority;
} ValentTaskClosure;

static inline void
valent_task_closure_free (gpointer data)
{
  ValentTaskClosure *closure = data;

  g_clear_object (&closure->task);
  g_clear_pointer (&closure, g_free);
}

static inline void
valent_task_closure_cancel (gpointer data)
{
  ValentTaskClosure *closure = data;

  if (G_IS_TASK (closure->task) && !g_task_get_completed (closure->task))
    {
      g_task_return_new_error (closure->task,
                               G_IO_ERROR,
                               G_IO_ERROR_CANCELLED,
                               "Operation cancelled");
    }

  g_clear_pointer (&closure, valent_task_closure_free);
}

/*
 * GThreadFunc
 */
static gpointer
valent_task_queue_loop (gpointer data)
{
  g_autoptr (GAsyncQueue) tasks = data;
  ValentTaskClosure *closure = NULL;

  while ((closure = g_async_queue_pop (tasks)))
    {
      unsigned int mode = closure->task_mode;

      if (G_IS_TASK (closure->task) && !g_task_get_completed (closure->task))
        {
          closure->task_func (closure->task,
                              g_task_get_source_object (closure->task),
                              g_task_get_task_data (closure->task),
                              g_task_get_cancellable (closure->task));

          if (mode == VALENT_TASK_CRITICAL && g_task_had_error (closure->task))
            mode = VALENT_TASK_TERMINAL;
        }

      g_clear_pointer (&closure, valent_task_closure_free);

      if (mode == VALENT_TASK_TERMINAL)
        break;
    }

  /* Cancel any queued tasks */
  g_async_queue_lock (tasks);

  while ((closure = g_async_queue_try_pop_unlocked (tasks)) != NULL)
    g_clear_pointer (&closure, valent_task_closure_cancel);

  g_async_queue_unlock (tasks);

  return NULL;
}

static inline int
valent_task_queue_sort (gconstpointer a,
                        gconstpointer b,
                        gpointer      user_data)
{
  const ValentTaskClosure *closure1 = a;
  const ValentTaskClosure *closure2 = b;

  return closure1->priority - closure2->priority;
}

static inline void
valent_task_queue_free (gpointer data)
{
  ValentTaskQueue *self = data;

  g_assert (VALENT_IS_TASK_QUEUE (self));

  g_async_queue_lock (self->tasks);

  if (!self->closed)
    {
      ValentTaskClosure *closure;

      closure = g_new0 (ValentTaskClosure, 1);
      closure->task_mode = VALENT_TASK_TERMINAL;

      self->closed = TRUE;
      g_async_queue_push_unlocked (self->tasks, closure);
    }

  g_async_queue_unlock (self->tasks);
  g_clear_pointer (&self->tasks, g_async_queue_unref);
}

/**
 * valent_task_queue_new:
 *
 * Create a new #ValentTaskQueue.
 *
 * Returns: (transfer full): a #ValentTaskQueue.
 */
ValentTaskQueue *
valent_task_queue_new (void)
{
  ValentTaskQueue *queue;
  g_autoptr (GThread) thread = NULL;
  g_autoptr (GError) error = NULL;

  queue = g_atomic_rc_box_new0 (ValentTaskQueue);
  queue->tasks = g_async_queue_new_full (valent_task_closure_cancel);
  thread = g_thread_try_new ("valent-task-queue",
                             valent_task_queue_loop,
                             g_async_queue_ref (queue->tasks),
                             &error);

  if G_UNLIKELY (error != NULL)
    {
      g_critical ("%s: %s", G_STRFUNC, error->message);
      queue->closed = TRUE;
    }

  return queue;
}

/**
 * valent_task_queue_ref:
 * @queue: a #ValentTaskQueue
 *
 * Increase the reference count of @queue.
 *
 * Returns: (transfer full): a #ValentTaskQueue.
 */
ValentTaskQueue *
valent_task_queue_ref (ValentTaskQueue *queue)
{
  g_return_val_if_fail (VALENT_IS_TASK_QUEUE (queue), NULL);

  return g_atomic_rc_box_acquire (queue);
}

/**
 * valent_task_queue_unref:
 * @queue: a #ValentTaskQueue
 *
 * Decreases the reference count of @queue.
 *
 * When the reference count drops to 0, @queue will signal its dedicated thread
 * to exit and free any resources.
 */
void
valent_task_queue_unref (ValentTaskQueue *queue)
{
  g_return_if_fail (VALENT_IS_TASK_QUEUE (queue));

  g_atomic_rc_box_release_full (queue, valent_task_queue_free);
}

/**
 * valent_task_queue_run_full:
 * @queue: a #ValentTaskQueue
 * @task: a #GTask
 * @task_func: (scope async): a #GTaskThreadFunc
 * @task_mode: a #ValentTaskMode
 *
 * Push @task and @task_func onto @queue with @task_mode.
 */
static void
valent_task_queue_run_full (ValentTaskQueue *self,
                            GTask           *task,
                            GTaskThreadFunc  task_func,
                            ValentTaskMode   task_mode)
{
  ValentTaskClosure *closure;

  g_assert (VALENT_IS_TASK_QUEUE (self));
  g_assert (G_IS_TASK (task));
  g_assert (task_func != NULL);

  closure = g_new0 (ValentTaskClosure, 1);
  closure->task = g_object_ref (task);
  closure->task_func = task_func;
  closure->task_mode = task_mode;
  closure->priority = g_task_get_priority (task);

  g_async_queue_lock (self->tasks);

  if (!self->closed)
    {
      self->closed = (closure->task_mode == VALENT_TASK_TERMINAL);
      g_async_queue_push_sorted_unlocked (self->tasks,
                                          g_steal_pointer (&closure),
                                          valent_task_queue_sort,
                                          NULL);
    }

  g_async_queue_unlock (self->tasks);

  g_clear_pointer (&closure, valent_task_closure_cancel);
}

/**
 * valent_task_queue_run:
 * @queue: a #ValentTaskQueue
 * @task: a #GTask
 * @task_func: (scope async): a #GTaskThreadFunc
 *
 * Push @task and @task_func onto @queue.
 */
void
valent_task_queue_run (ValentTaskQueue *queue,
                       GTask           *task,
                       GTaskThreadFunc  task_func)
{
  g_return_if_fail (VALENT_IS_TASK_QUEUE (queue));
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task_func != NULL);

  valent_task_queue_run_full (queue, task, task_func, VALENT_TASK_SEQUENTIAL);
}

/**
 * valent_task_queue_run_check:
 * @queue: a #ValentTaskQueue
 * @task: a #GTask
 * @task_func: (scope async): a #GTaskThreadFunc
 *
 * A variant of valent_task_queue_run() that checks if a @task succeeds.
 *
 * If @task reports an error, any queued tasks will be cancelled and no new
 * tasks will be accepted by @queue.
 *
 * Be aware that tasks are executed in order of priority, so tasks queued before
 * @task may be affected if it has a higher priority.
 */
void
valent_task_queue_run_check (ValentTaskQueue *queue,
                             GTask           *task,
                             GTaskThreadFunc  task_func)
{
  g_return_if_fail (VALENT_IS_TASK_QUEUE (queue));
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task_func != NULL);

  valent_task_queue_run_full (queue, task, task_func, VALENT_TASK_CRITICAL);
}

/**
 * valent_task_queue_run_close:
 * @queue: a #ValentTaskQueue
 * @task: a #GTask
 * @task_func: (scope async): a #GTaskThreadFunc
 *
 * A variant of valent_task_queue_run() that halts @queue when @task completes.
 *
 * When @task completes, any queued tasks will be cancelled and no new
 * tasks will be accepted by @queue.
 *
 * Be aware that tasks are executed in order of priority, so @task may be run
 * before tasks already waiting in @queue.
 */
void
valent_task_queue_run_close (ValentTaskQueue *queue,
                             GTask           *task,
                             GTaskThreadFunc  task_func)
{
  g_return_if_fail (VALENT_IS_TASK_QUEUE (queue));
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task_func != NULL);

  valent_task_queue_run_full (queue, task, task_func, VALENT_TASK_TERMINAL);
}

/**
 * valent_task_queue_run_sync:
 * @queue: a #ValentTaskQueue
 * @task: a #GTask
 * @task_func: (scope async): a #GTaskThreadFunc
 *
 * Push @task and @task_func onto @queue and block until @task completes.
 *
 * Be aware that tasks are executed in order of priority, so tasks queued after
 * @task may be executed first if they have a higher priority.
 */
void
valent_task_queue_run_sync (ValentTaskQueue *queue,
                            GTask           *task,
                            GTaskThreadFunc  task_func)
{
  GMainContext *task_context;

  g_return_if_fail (VALENT_IS_TASK_QUEUE (queue));
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task_func != NULL);

  valent_task_queue_run_full (queue, task, task_func, VALENT_TASK_SEQUENTIAL);

  /* Iterate @task's main context until it completes */
  task_context = g_task_get_context (task);

  while (!g_task_get_completed (task))
    g_main_context_iteration (task_context, TRUE);
}

