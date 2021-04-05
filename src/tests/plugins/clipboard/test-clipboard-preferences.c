#include <libvalent-test.h>
#include <libvalent-ui.h>


static void
test_clipboard_plugin_preferences (void)
{
  PeasEngine *engine;
  PeasPluginInfo *info;
  PeasExtension *prefs;
  g_autofree char *plugin_context = NULL;

  engine = valent_get_engine ();
  info = peas_engine_get_plugin_info (engine, "clipboard");
  prefs = peas_engine_create_extension (engine,
                                        info,
                                        VALENT_TYPE_PLUGIN_PREFERENCES,
                                        "plugin-context", "test-device",
                                        NULL);
  g_object_ref_sink (prefs);

  g_object_get (prefs, "plugin-context", &plugin_context, NULL);
  g_assert_cmpstr (plugin_context, ==, "test-device");

  g_object_unref (prefs);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_ui_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/plugins/clipboard/preferences",
                   test_clipboard_plugin_preferences);

  return g_test_run ();
}

