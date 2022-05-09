// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <libvalent-clipboard.h>
#include <libvalent-core.h>
#include <libvalent-test.h>


typedef struct
{
  ValentClipboard *clipboard;
  GMainLoop       *loop;
  gpointer         data;
} ClipboardComponentFixture;

static void
clipboard_component_fixture_set_up (ClipboardComponentFixture *fixture,
                                    gconstpointer              user_data)
{
  fixture->clipboard = valent_clipboard_get_default ();
  fixture->loop = g_main_loop_new (NULL, FALSE);
}

static void
clipboard_component_fixture_tear_down (ClipboardComponentFixture *fixture,
                                       gconstpointer              user_data)
{
  v_assert_finalize_object (fixture->clipboard);
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
}

static void
on_changed (ValentClipboardAdapter    *adapter,
            ClipboardComponentFixture *fixture)
{
  fixture->data = adapter;
}

static void
adapter_get_text_cb (ValentClipboardAdapter    *adapter,
                     GAsyncResult              *result,
                     ClipboardComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_adapter_get_text_finish (adapter, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
get_text_cb (ValentClipboard           *clipboard,
             GAsyncResult              *result,
             ClipboardComponentFixture *fixture)
{
  GError *error = NULL;

  fixture->data = valent_clipboard_get_text_finish (clipboard, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
test_clipboard_component_adapter (ClipboardComponentFixture *fixture,
                                  gconstpointer              user_data)
{
  ValentClipboardAdapter *adapter;
  g_autofree char *text = NULL;
  PeasPluginInfo *info;

  while ((adapter = valent_mock_clipboard_adapter_get_instance ()) == NULL)
    continue;

  /* Properties */
  g_object_get (adapter,
                "plugin-info", &info,
                NULL);
  g_assert_nonnull (info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Signals */
  g_signal_connect (adapter,
                    "changed",
                    G_CALLBACK (on_changed),
                    fixture);

  valent_clipboard_adapter_emit_changed (adapter);
  g_assert_true (fixture->data == adapter);
  fixture->data = NULL;

  /* Methods */
  text = g_uuid_string_random ();
  valent_clipboard_set_text (fixture->clipboard, text);

  valent_clipboard_adapter_get_text_async (adapter,
                                           NULL,
                                           (GAsyncReadyCallback)adapter_get_text_cb,
                                           fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpstr (fixture->data, ==, text);
  g_clear_pointer (&fixture->data, g_free);
}

static void
test_clipboard_component_self (ClipboardComponentFixture *fixture,
                               gconstpointer              user_data)
{
  g_autofree char *text = NULL;

  text = g_uuid_string_random ();
  valent_clipboard_set_text (fixture->clipboard, text);

  valent_clipboard_get_text_async (fixture->clipboard,
                                   NULL,
                                   (GAsyncReadyCallback)get_text_cb,
                                   fixture);
  g_main_loop_run (fixture->loop);

  g_assert_cmpstr (fixture->data, ==, text);
  g_clear_pointer (&fixture->data, g_free);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/components/clipboard/adapter",
              ClipboardComponentFixture, NULL,
              clipboard_component_fixture_set_up,
              test_clipboard_component_adapter,
              clipboard_component_fixture_tear_down);

  g_test_add ("/components/clipboard/self",
              ClipboardComponentFixture, NULL,
              clipboard_component_fixture_set_up,
              test_clipboard_component_self,
              clipboard_component_fixture_tear_down);

  return g_test_run ();
}
