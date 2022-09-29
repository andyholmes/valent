// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-core.h>
#include <libvalent-test.h>


static void
on_destroy (ValentObject *object,
            gboolean     *destroyed)
{
  g_assert_true (VALENT_IS_MAIN_THREAD());
  g_assert_true (valent_object_in_destruction (object));

  if (destroyed)
    *destroyed = TRUE;
}

static void
on_notify (ValentObject *object,
           GParamSpec   *pspec,
           gboolean     *notified)
{
  g_assert_true (VALENT_IS_MAIN_THREAD());
  g_assert_true (G_IS_PARAM_SPEC (pspec));

  if (notified)
    *notified = TRUE;
}

static void
test_object_basic (void)
{
  g_autoptr (ValentObject) object = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GCancellable) cancellable_out = NULL;
  gboolean destroyed = FALSE;

  /* Construct and dispose */
  object = g_object_new (VALENT_TYPE_OBJECT, NULL);
  g_signal_connect (object,
                    "destroy",
                    G_CALLBACK (on_destroy),
                    &destroyed);
  g_clear_object (&object);
  g_assert_true (destroyed);
  destroyed = FALSE;

  /* Construct and destroy */
  object = g_object_new (VALENT_TYPE_OBJECT, NULL);
  g_signal_connect (object,
                    "destroy",
                    G_CALLBACK (on_destroy),
                    &destroyed);
  valent_object_destroy (object);
  g_assert_true (destroyed);
  g_clear_object (&object);
  destroyed = FALSE;

  /* Construct cancellable */
  cancellable = g_cancellable_new ();
  object = g_object_new (VALENT_TYPE_OBJECT,
                         "cancellable", cancellable,
                         NULL);

  g_object_get (object,
                "cancellable", &cancellable_out,
                NULL);
  g_assert_true (cancellable == cancellable_out);

  g_clear_object (&object);
  g_assert_true (g_cancellable_is_cancelled (cancellable));
}


static gpointer
construct_thread_func (gboolean *destroyed)
{
  g_autoptr (ValentObject) object = NULL;
  g_autoptr (GCancellable) cancellable = NULL;

  cancellable = g_cancellable_new ();
  object = g_object_new (VALENT_TYPE_OBJECT,
                         "cancellable", cancellable,
                         NULL);

  g_signal_connect (object,
                    "destroy",
                    G_CALLBACK (on_destroy),
                    destroyed);
  valent_object_destroy (object);

  return g_steal_pointer (&object);
}

static void
test_object_construct_thread (void)
{
  g_autoptr (ValentObject) object = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GThread) thread = NULL;
  gboolean destroyed = FALSE;

  thread = g_thread_new ("valent-object-construct",
                         (GThreadFunc)construct_thread_func,
                         &destroyed);
  object = g_thread_join (thread);
  cancellable = valent_object_ref_cancellable (object);

  while (!destroyed)
    g_main_context_iteration (NULL, FALSE);

  g_assert_true (g_cancellable_is_cancelled (cancellable));
}


static gpointer
dispose_thread_func (ValentObject *object)
{
  g_object_unref (object);

  return NULL;
}

static void
test_object_dispose_thread (void)
{
  g_autoptr (ValentObject) object = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GThread) thread = NULL;
  gboolean destroyed = FALSE;

  cancellable = g_cancellable_new ();
  object = g_object_new (VALENT_TYPE_OBJECT,
                         "cancellable", cancellable,
                         NULL);

  g_signal_connect (object,
                    "destroy",
                    G_CALLBACK (on_destroy),
                    &destroyed);

  thread = g_thread_new ("valent-object-dispose",
                         (GThreadFunc)dispose_thread_func,
                         g_steal_pointer (&object));

  while (!destroyed)
    g_main_context_iteration (NULL, FALSE);

  g_assert_null (g_thread_join (thread));
  g_assert_true (g_cancellable_is_cancelled (cancellable));
}


static gpointer
destroy_thread_func (ValentObject *object)
{
  g_autoptr (GCancellable) cancellable = NULL;

  cancellable = valent_object_ref_cancellable (object);
  valent_object_destroy (object);
  g_object_unref (object);

  return NULL;
}

static void
test_object_destroy_thread (void)
{
  g_autoptr (ValentObject) object = NULL;
  g_autoptr (GCancellable) cancellable = NULL;
  g_autoptr (GThread) thread = NULL;
  gboolean destroyed = FALSE;

  cancellable = g_cancellable_new ();
  object = g_object_new (VALENT_TYPE_OBJECT,
                         "cancellable", cancellable,
                         NULL);

  g_signal_connect (object,
                    "destroy",
                    G_CALLBACK (on_destroy),
                    &destroyed);

  thread = g_thread_new ("valent-object-destroy",
                         (GThreadFunc)destroy_thread_func,
                         g_steal_pointer (&object));

  while (!destroyed)
    g_main_context_iteration (NULL, FALSE);

  g_assert_null (g_thread_join (thread));
  g_assert_true (g_cancellable_is_cancelled (cancellable));
}


static gpointer
notify_thread_func (ValentObject *object)
{
  valent_object_notify (object, "cancellable");

  return NULL;
}

static void
test_object_notify_thread (void)
{
  g_autoptr (ValentObject) object = NULL;
  g_autoptr (GThread) thread = NULL;
  gboolean notified = FALSE;

  object = g_object_new (VALENT_TYPE_OBJECT, NULL);
  g_signal_connect (object,
                    "notify::cancellable",
                    G_CALLBACK (on_notify),
                    &notified);

  thread = g_thread_new ("valent-object-notify",
                         (GThreadFunc)notify_thread_func,
                         object);

  while (!notified)
    g_main_context_iteration (NULL, FALSE);

  g_assert_null (g_thread_join (thread));
}


int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/core/object/basic",
                   test_object_basic);

  g_test_add_func ("/core/object/construct-thread",
                   test_object_construct_thread);

  g_test_add_func ("/core/object/dispose-thread",
                   test_object_dispose_thread);

  g_test_add_func ("/core/object/destroy-thread",
                   test_object_destroy_thread);

  g_test_add_func ("/core/object/notify-thread",
                   test_object_notify_thread);

  g_test_run ();
}
