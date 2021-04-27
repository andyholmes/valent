#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-session.h>
#include <libvalent-test.h>


typedef struct
{
  ValentSession *session;
  gpointer       data;
} SessionComponentFixture;

static void
session_component_fixture_set_up (SessionComponentFixture *fixture,
                                  gconstpointer            user_data)
{
  fixture->session = valent_session_get_default ();
}

static void
session_component_fixture_tear_down (SessionComponentFixture *fixture,
                                     gconstpointer            user_data)
{
  g_assert_finalize_object (fixture->session);
}

static void
on_changed (ValentSessionAdapter    *adapter,
            SessionComponentFixture *fixture)
{
  fixture->data = adapter;
}

static void
test_session_component_provider (SessionComponentFixture *fixture,
                                 gconstpointer            user_data)
{
  g_autoptr (GPtrArray) extensions = NULL;
  ValentSessionAdapter *provider;
  gboolean active, locked;

  /* Wait for valent_session_adapter_load_async() to resolve */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->session));
  provider = g_ptr_array_index (extensions, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Compare Device & Aggregator */
  g_object_get (provider,
                "active", &active,
                "locked", &locked,
                NULL);

  g_assert_false (active);
  g_assert_false (locked);

  /* Change adapter */
  g_signal_connect (provider,
                    "changed",
                    G_CALLBACK (on_changed),
                    fixture);

  g_object_set (provider,
                "locked", !locked,
                NULL);

  g_assert_true (valent_session_adapter_get_locked (provider));
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;
}

static void
test_session_component_self (SessionComponentFixture *fixture,
                           gconstpointer          user_data)
{
  g_autoptr (GPtrArray) extensions = NULL;
  ValentSessionAdapter *provider;
  gboolean session_active, session_locked;
  gboolean adapter_active, adapter_locked;
  PeasPluginInfo *info;

  /* Wait for valent_session_adapter_load_async() to resolve */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->session));
  provider = g_ptr_array_index (extensions, 0);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Compare session & adapter */
  session_active = valent_session_get_active (fixture->session);
  session_locked = valent_session_get_locked (fixture->session);

  g_object_get (provider,
                "active",      &adapter_active,
                "locked",      &adapter_locked,
                "plugin-info", &info,
                NULL);

  g_assert_true (session_active == adapter_active);
  g_assert_true (session_locked == adapter_locked);
  g_assert_nonnull (info);

  g_boxed_free (PEAS_TYPE_PLUGIN_INFO, info);

  /* Change session */
  g_signal_connect (provider,
                    "changed",
                    G_CALLBACK (on_changed),
                    fixture);

  valent_session_set_locked (fixture->session, !session_locked);

  g_assert_true (valent_session_get_locked (fixture->session));
  g_assert_true (fixture->data == provider);
  fixture->data = NULL;
}

static void
test_session_component_dispose (SessionComponentFixture *fixture,
                              gconstpointer          user_data)
{
  GPtrArray *extensions;
  ValentSessionAdapter *provider;
  PeasEngine *engine;
  g_autoptr (GSettings) settings = NULL;

  /* Add a device to the provider */
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->session));
  provider = g_ptr_array_index (extensions, 0);
  g_ptr_array_unref (extensions);

  /* Wait for provider to resolve */
  valent_session_adapter_emit_changed (provider);

  while (g_main_context_iteration (NULL, FALSE))
    continue;

  /* Disable/Enable the provider */
  settings = valent_component_new_settings ("session", "mock");

  g_settings_set_boolean (settings, "enabled", FALSE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->session));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);

  g_settings_set_boolean (settings, "enabled", TRUE);
  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->session));
  g_assert_cmpuint (extensions->len, ==, 1);
  g_ptr_array_unref (extensions);

  /* Unload the provider */
  engine = valent_get_engine ();
  peas_engine_unload_plugin (engine, peas_engine_get_plugin_info (engine, "mock"));

  extensions = valent_component_get_extensions (VALENT_COMPONENT (fixture->session));
  g_assert_cmpuint (extensions->len, ==, 0);
  g_ptr_array_unref (extensions);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add ("/components/session/provider",
              SessionComponentFixture, NULL,
              session_component_fixture_set_up,
              test_session_component_provider,
              session_component_fixture_tear_down);

  g_test_add ("/components/session/self",
              SessionComponentFixture, NULL,
              session_component_fixture_set_up,
              test_session_component_self,
              session_component_fixture_tear_down);

  g_test_add ("/components/session/dispose",
              SessionComponentFixture, NULL,
              session_component_fixture_set_up,
              test_session_component_dispose,
              session_component_fixture_tear_down);

  return g_test_run ();
}
