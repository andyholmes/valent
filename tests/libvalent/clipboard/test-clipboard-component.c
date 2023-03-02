// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


typedef struct
{
  ValentClipboard        *clipboard;
  ValentClipboardAdapter *adapter;
  GMainLoop              *loop;
  gpointer                data;
} ClipboardComponentFixture;

static void
clipboard_component_fixture_set_up (ClipboardComponentFixture *fixture,
                                    gconstpointer              user_data)
{
  fixture->clipboard = valent_clipboard_get_default ();
  fixture->adapter = valent_test_await_adapter (fixture->clipboard);
  fixture->loop = g_main_loop_new (NULL, FALSE);

  g_object_ref (fixture->adapter);
}

static void
clipboard_component_fixture_tear_down (ClipboardComponentFixture *fixture,
                                       gconstpointer              user_data)
{
  v_assert_finalize_object (fixture->clipboard);
  v_await_finalize_object (fixture->adapter);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
on_changed (ValentClipboardAdapter    *adapter,
            ClipboardComponentFixture *fixture)
{
  fixture->data = adapter;
}

static void
valent_clipboard_adapter_read_bytes_cb (ValentClipboardAdapter    *adapter,
                                        GAsyncResult              *result,
                                        ClipboardComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_adapter_read_bytes_finish (adapter,
                                                              result,
                                                              &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_adapter_write_bytes_cb (ValentClipboardAdapter    *adapter,
                                         GAsyncResult              *result,
                                         ClipboardComponentFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_clipboard_adapter_write_bytes_finish (adapter, result, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_adapter_read_text_cb (ValentClipboardAdapter    *adapter,
                                       GAsyncResult              *result,
                                       ClipboardComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_adapter_read_text_finish (adapter,
                                                             result,
                                                             &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_adapter_write_text_cb (ValentClipboardAdapter    *adapter,
                                        GAsyncResult              *result,
                                        ClipboardComponentFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_clipboard_adapter_write_text_finish (adapter, result, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_read_bytes_cb (ValentClipboard           *clipboard,
                                GAsyncResult              *result,
                                ClipboardComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_read_bytes_finish (clipboard, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_write_bytes_cb (ValentClipboard           *clipboard,
                                 GAsyncResult              *result,
                                 ClipboardComponentFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_clipboard_write_bytes_finish (clipboard, result, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_read_text_cb (ValentClipboard           *clipboard,
                               GAsyncResult              *result,
                               ClipboardComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_read_text_finish (clipboard, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
valent_clipboard_write_text_cb (ValentClipboard           *clipboard,
                                GAsyncResult              *result,
                                ClipboardComponentFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_clipboard_write_text_finish (clipboard, result, &error);
  g_assert_true (ret);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
test_clipboard_component_adapter (ClipboardComponentFixture *fixture,
                                  gconstpointer              user_data)
{
  PeasPluginInfo *info;
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *text = NULL;
  g_auto (GStrv) mimetypes = NULL;
  gint64 timestamp = 0;

  /* Adapter Properties */
  g_object_get (fixture->adapter,
                "plugin-info", &info,
                NULL);
  g_assert_nonnull (info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Data can be written */
  text = g_uuid_string_random ();
  bytes = g_bytes_new_take (text, strlen (text) + 1);
  text = NULL;
  valent_clipboard_adapter_write_bytes (fixture->adapter,
                                        "text/plain;charset=utf-8",
                                        bytes,
                                        NULL,
                                        (GAsyncReadyCallback)valent_clipboard_adapter_write_bytes_cb,
                                        fixture);
  g_main_loop_run (fixture->loop);

  /* Data can be read */
  valent_clipboard_adapter_read_bytes (fixture->adapter,
                                       "text/plain;charset=utf-8",
                                       NULL,
                                       (GAsyncReadyCallback)valent_clipboard_adapter_read_bytes_cb,
                                       fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpmem (g_bytes_get_data (bytes, NULL),
                   g_bytes_get_size (bytes),
                   g_bytes_get_data (fixture->data, NULL),
                   g_bytes_get_size (fixture->data));
  g_clear_pointer (&fixture->data, g_bytes_unref);

  /* Timestamp is updated */
  timestamp = valent_clipboard_adapter_get_timestamp (fixture->adapter);
  g_assert_cmpint (timestamp, !=, 0);

  /* Mimetypes are updated */
  mimetypes = valent_clipboard_adapter_get_mimetypes (fixture->adapter);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Text can be written */
  text = g_uuid_string_random ();
  valent_clipboard_adapter_write_text (fixture->adapter,
                                       text,
                                       NULL,
                                       (GAsyncReadyCallback)valent_clipboard_adapter_write_text_cb,
                                       fixture);
  g_main_loop_run (fixture->loop);

  /* Text can be read */
  valent_clipboard_adapter_read_text (fixture->adapter,
                                      NULL,
                                      (GAsyncReadyCallback)valent_clipboard_adapter_read_text_cb,
                                      fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpstr (fixture->data, ==, text);
  g_clear_pointer (&fixture->data, g_free);
  g_clear_pointer (&text, g_free);

  /* Timestamp is updated */
  timestamp = valent_clipboard_adapter_get_timestamp (fixture->adapter);
  g_assert_cmpint (timestamp, !=, 0);

  /* Mimetypes are updated */
  mimetypes = valent_clipboard_adapter_get_mimetypes (fixture->adapter);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Signals are emitted from adapter */
  g_signal_connect (fixture->adapter,
                    "changed",
                    G_CALLBACK (on_changed),
                    fixture);

  valent_clipboard_adapter_changed (fixture->adapter);
  g_assert_true (fixture->data == fixture->adapter);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->adapter, fixture);
}

static void
test_clipboard_component_self (ClipboardComponentFixture *fixture,
                               gconstpointer              user_data)
{
  g_autoptr (GBytes) bytes = NULL;
  g_autofree char *text = NULL;
  g_auto (GStrv) mimetypes = NULL;
  gint64 timestamp = 0;

  /* Data can be written */
  text = g_uuid_string_random ();
  bytes = g_bytes_new_take (text, strlen (text) + 1);
  text = NULL;
  valent_clipboard_write_bytes (fixture->clipboard,
                                "text/plain;charset=utf-8",
                                bytes,
                                NULL,
                                (GAsyncReadyCallback)valent_clipboard_write_bytes_cb,
                                fixture);
  g_main_loop_run (fixture->loop);

  /* Data can be read */
  valent_clipboard_read_bytes (fixture->clipboard,
                               "text/plain;charset=utf-8",
                               NULL,
                               (GAsyncReadyCallback)valent_clipboard_read_bytes_cb,
                               fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpmem (g_bytes_get_data (bytes, NULL),
                   g_bytes_get_size (bytes),
                   g_bytes_get_data (fixture->data, NULL),
                   g_bytes_get_size (fixture->data));
  g_clear_pointer (&fixture->data, g_bytes_unref);

  /* Timestamp is updated */
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  /* Mimetypes are updated */
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Text can be written */
  text = g_uuid_string_random ();
  valent_clipboard_write_text (fixture->clipboard,
                               text,
                               NULL,
                               (GAsyncReadyCallback)valent_clipboard_write_text_cb,
                               fixture);
  g_main_loop_run (fixture->loop);

  /* Text can be read */
  valent_clipboard_read_text (fixture->clipboard,
                              NULL,
                              (GAsyncReadyCallback)valent_clipboard_read_text_cb,
                              fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpstr (fixture->data, ==, text);
  g_clear_pointer (&fixture->data, g_free);
  g_clear_pointer (&text, g_free);

  /* Timestamp is updated */
  timestamp = valent_clipboard_get_timestamp (fixture->clipboard);
  g_assert_cmpint (timestamp, !=, 0);

  /* Mimetypes are updated */
  mimetypes = valent_clipboard_get_mimetypes (fixture->clipboard);
  g_assert_nonnull (mimetypes);
  g_assert_true (g_strv_contains ((const char * const *)mimetypes,
                                  "text/plain;charset=utf-8"));
  g_clear_pointer (&mimetypes, g_strfreev);

  /* Signals are propagated from adapter */
  g_signal_connect (fixture->clipboard,
                    "changed",
                    G_CALLBACK (on_changed),
                    fixture);

  valent_clipboard_adapter_changed (fixture->adapter);
  g_assert_true (fixture->data == fixture->clipboard);
  fixture->data = NULL;

  g_signal_handlers_disconnect_by_data (fixture->clipboard, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/clipboard/adapter",
              ClipboardComponentFixture, NULL,
              clipboard_component_fixture_set_up,
              test_clipboard_component_adapter,
              clipboard_component_fixture_tear_down);

  g_test_add ("/libvalent/clipboard/self",
              ClipboardComponentFixture, NULL,
              clipboard_component_fixture_set_up,
              test_clipboard_component_self,
              clipboard_component_fixture_tear_down);

  return g_test_run ();
}
