// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
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
  g_assert_finalize_object (fixture->clipboard);
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
test_clipboard_component_provider (ClipboardComponentFixture *fixture,
                                   gconstpointer              user_data)
{
  g_autoptr (GPtrArray) extensions = NULL;
  ValentClipboardAdapter *provider;
  g_autofree char *text = NULL;
  PeasPluginInfo *info;

  /* Wait for valent_clipboard_device_provider_load_async() to resolve */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->clipboard));
  provider = g_ptr_array_index (extensions, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Properties */
  g_object_get (provider,
                "plugin-info", &info,
                NULL);
  g_assert_nonnull (info);
  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Signals */
  g_signal_connect (provider,
                    "changed",
                    G_CALLBACK (on_changed),
                    fixture);

  valent_clipboard_adapter_emit_changed (provider);
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;

  /* Methods */
  text = g_uuid_string_random ();
  valent_clipboard_set_text (fixture->clipboard, text);

  valent_clipboard_adapter_get_text_async (provider,
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

static void
test_clipboard_component_dispose (ClipboardComponentFixture *fixture,
                                  gconstpointer              user_data)
{
  GPtrArray *extensions;
  PeasEngine *engine;
  g_autoptr (GSettings) settings = NULL;

  /* Add a store to the provider */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->clipboard));
  g_assert_cmpuint (extensions->len, ==, 1);
  g_ptr_array_unref (extensions);

  /* Wait for provider to resolve */
  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Disable/Enable the provider */
  settings = valent_component_new_settings ("clipboard", "mock");

  g_settings_set_boolean (settings, "enabled", FALSE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->clipboard));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);

  g_settings_set_boolean (settings, "enabled", TRUE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->clipboard));
  g_assert_cmpuint (extensions->len, ==, 1);
  g_ptr_array_unref (extensions);

  /* Unload the provider */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->clipboard));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/components/clipboard/provider",
              ClipboardComponentFixture, NULL,
              clipboard_component_fixture_set_up,
              test_clipboard_component_provider,
              clipboard_component_fixture_tear_down);

  g_test_add ("/components/clipboard/self",
              ClipboardComponentFixture, NULL,
              clipboard_component_fixture_set_up,
              test_clipboard_component_self,
              clipboard_component_fixture_tear_down);

  g_test_add ("/components/clipboard/dispose",
              ClipboardComponentFixture, NULL,
              clipboard_component_fixture_set_up,
              test_clipboard_component_dispose,
              clipboard_component_fixture_tear_down);

  return g_test_run ();
}
