// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


typedef struct
{
  ValentTaskQueue *queue;
  GMainLoop       *loop;
  unsigned int     n_tasks;
} TaskQueueFixture;


static void
task_queue_fixture_set_up (TaskQueueFixture *fixture,
                           gconstpointer     user_data)
{
  fixture->queue = valent_task_queue_new ();
  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->n_tasks = MAX (g_get_num_processors (), 4);
}

static void
task_queue_fixture_tear_down (TaskQueueFixture *fixture,
                              gconstpointer     user_data)
{
  g_clear_pointer (&fixture->queue, valent_task_queue_unref);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
task_success_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  TaskQueueFixture *fixture = user_data;
  GError *error = NULL;

  g_task_propagate_boolean (G_TASK (result), &error);
  g_assert_no_error (error);

  if (--fixture->n_tasks == 0)
    g_main_loop_quit (fixture->loop);
}

static void
task_success_func (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  g_usleep (10 * G_TIME_SPAN_MILLISECOND);

  g_task_return_boolean (task, TRUE);
}

static void
task_failure_cb (GObject      *object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  TaskQueueFixture *fixture = user_data;
  g_autoptr (GError) error = NULL;

  g_task_propagate_boolean (G_TASK (result), &error);
  g_assert_nonnull (error);

  if (--fixture->n_tasks == 0)
    g_main_loop_quit (fixture->loop);
}

static void
task_failure_func (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "Intentional failure");
}

static gboolean
task_queue_unref_idle (gpointer data)
{
  TaskQueueFixture *fixture = data;

  g_clear_pointer (&fixture->queue, valent_task_queue_unref);

  return G_SOURCE_REMOVE;
}


static void
test_task_queue_basic (TaskQueueFixture *fixture,
                       gconstpointer     user_data)
{
  g_assert_true (VALENT_IS_TASK_QUEUE (fixture->queue));

  valent_task_queue_ref (fixture->queue);
  valent_task_queue_unref (fixture->queue);
}

static void
test_task_queue_check (TaskQueueFixture *fixture,
                       gconstpointer     user_data)
{
  GTask *task;

  /* Check task (return failure, expect failure) */
  task = g_task_new (NULL, NULL, task_failure_cb, fixture);
  valent_task_queue_run_check (fixture->queue, task, task_failure_func);
  g_clear_object (&task);

  /* Standard tasks (return success, expect failure) */
  for (unsigned int i = 0; i < fixture->n_tasks; i++)
    {
      task = g_task_new (NULL, NULL, task_failure_cb, fixture);
      valent_task_queue_run (fixture->queue, task, task_success_func);
      g_clear_object (&task);
    }

  fixture->n_tasks += 1;
  g_clear_pointer (&fixture->queue, valent_task_queue_unref);
  g_main_loop_run (fixture->loop);
}

static void
test_task_queue_close (TaskQueueFixture *fixture,
                       gconstpointer     user_data)
{
  GTask *task;

  /* Close task (return success, expect success) */
  task = g_task_new (NULL, NULL, task_success_cb, fixture);
  valent_task_queue_run_close (fixture->queue, task, task_success_func);
  g_clear_object (&task);

  /* Standard tasks (return success, expect failure) */
  for (unsigned int i = 0; i < fixture->n_tasks; i++)
    {
      task = g_task_new (NULL, NULL, task_failure_cb, fixture);
      valent_task_queue_run (fixture->queue, task, task_success_func);
      g_clear_object (&task);
    }

  fixture->n_tasks += 1;
  g_main_loop_run (fixture->loop);
}

static void
test_task_queue_full (TaskQueueFixture *fixture,
                      gconstpointer     user_data)
{
  GTask *task;

  /* Check task (return success, expect success) */
  task = g_task_new (NULL, NULL, task_success_cb, fixture);
  valent_task_queue_run_check (fixture->queue, task, task_success_func);
  g_clear_object (&task);

  /* Standard tasks (return success, expect success) */
  for (unsigned int i = 0; i < fixture->n_tasks; i++)
    {
      task = g_task_new (NULL, NULL, task_success_cb, fixture);
      valent_task_queue_run (fixture->queue, task, task_success_func);
      g_clear_object (&task);
    }

  /* Sync task (return success, expect success) */
  task = g_task_new (NULL, NULL, task_success_cb, fixture);
  valent_task_queue_run_sync (fixture->queue, task, task_success_func);
  g_clear_object (&task);

  /* Close task (return success, expect success) */
  task = g_task_new (NULL, NULL, task_success_cb, fixture);
  valent_task_queue_run_close (fixture->queue, task, task_success_func);
  g_clear_object (&task);

  fixture->n_tasks += 3;
  g_main_loop_run (fixture->loop);
}

static void
test_task_queue_dispose (TaskQueueFixture *fixture,
                         gconstpointer     user_data)
{
  GTask *task;

  /* Standard tasks (return success, expect success) */
  for (unsigned int i = 0; i < fixture->n_tasks; i++)
    {
      task = g_task_new (NULL, NULL, task_success_cb, fixture);
      valent_task_queue_run (fixture->queue, task, task_success_func);
      g_clear_object (&task);
    }

  g_clear_pointer (&fixture->queue, valent_task_queue_unref);
  g_main_loop_run (fixture->loop);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add ("/core/task-queue/basic",
              TaskQueueFixture, NULL,
              task_queue_fixture_set_up,
              test_task_queue_basic,
              task_queue_fixture_tear_down);

  g_test_add ("/core/task-queue/check",
              TaskQueueFixture, NULL,
              task_queue_fixture_set_up,
              test_task_queue_check,
              task_queue_fixture_tear_down);

  g_test_add ("/core/task-queue/close",
              TaskQueueFixture, NULL,
              task_queue_fixture_set_up,
              test_task_queue_close,
              task_queue_fixture_tear_down);

  g_test_add ("/core/task-queue/full",
              TaskQueueFixture, NULL,
              task_queue_fixture_set_up,
              test_task_queue_full,
              task_queue_fixture_tear_down);

  g_test_add ("/core/task-queue/dispose",
              TaskQueueFixture, NULL,
              task_queue_fixture_set_up,
              test_task_queue_dispose,
              task_queue_fixture_tear_down);

  return g_test_run ();
}

