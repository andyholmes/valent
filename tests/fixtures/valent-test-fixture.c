// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>

#ifdef HAVE_WALBOTTLE
# include <libwalbottle/wbl-schema.h>
#endif /* HAVE_WALBOTTLE */

#include "valent-device-private.h"
#include "valent-test-fixture.h"
#include "valent-test-utils.h"


/**
 * ValentTestFixture:
 *
 * A test fixture for Valent.
 *
 * `ValentTestFixture` is a fixture for tests that require a
 * [class@Valent.Device] with a channel pair.
 */
G_DEFINE_BOXED_TYPE (ValentTestFixture, valent_test_fixture,
                                        valent_test_fixture_ref,
                                        valent_test_fixture_unref)


static void
expect_packet_cb (ValentChannel  *channel,
                  GAsyncResult   *result,
                  JsonNode      **packet)
{
  g_autoptr (GError) error = NULL;

  *packet = valent_channel_read_packet_finish (channel, result, &error);

  if (error != NULL)
    g_critical ("%s(): %s", G_STRFUNC, error->message);
}

static void
valent_test_fixture_free (gpointer data)
{
  ValentTestFixture *fixture = data;

  valent_test_fixture_clear (fixture, NULL);
}

/**
 * valent_test_fixture_init:
 * @fixture: a `ValentTestFixture`
 * @user_data: a file path
 *
 * A fixture setup function.
 */
void
valent_test_fixture_init (ValentTestFixture *fixture,
                          gconstpointer      user_data)
{
  PeasEngine *engine = valent_get_plugin_engine ();
  const char *path = (const char *)user_data;
  g_autofree ValentChannel **channels = NULL;
  g_auto (GStrv) plugins = NULL;
  JsonNode *identity, *peer_identity;

  g_assert (path != NULL && *path != '\0');

  fixture->packets = valent_test_load_json (path);
  identity = valent_test_fixture_lookup_packet (fixture, "identity");
  peer_identity = valent_test_fixture_lookup_packet (fixture, "peer-identity");

  /* Init channels & device */
  valent_test_channel_pair (identity,
                            peer_identity,
                            &fixture->channel,
                            &fixture->endpoint);
  fixture->device = valent_device_new_full (peer_identity, NULL);
  valent_device_set_paired (fixture->device, TRUE);

  /* Init settings */
  plugins = valent_device_get_plugins (fixture->device);

  for (unsigned int i = 0; plugins[i] != NULL; i++)
    {
      PeasPluginInfo *plugin_info;
      const char *module_name = plugins[i];
      ValentContext *context = NULL;
      g_autoptr (ValentContext) plugin_context = NULL;

      if (g_str_equal (module_name, "mock") ||
          g_str_equal (module_name, "packetless"))
        continue;

      context = valent_device_get_context (fixture->device);
      plugin_info = peas_engine_get_plugin_info (engine, module_name);
      plugin_context = valent_context_get_plugin_context (context, plugin_info);
      fixture->settings = valent_context_get_plugin_settings (plugin_context,
                                                              plugin_info,
                                                              "X-DevicePluginSettings");
      break;
    }
}

/**
 * valent_test_fixture_clear:
 * @fixture: a `ValentTestFixture`
 * @user_data: a file path
 *
 * A fixture tear-down function.
 */
void
valent_test_fixture_clear (ValentTestFixture *fixture,
                           gconstpointer      user_data)
{
  g_clear_pointer (&fixture->packets, json_node_unref);
  g_clear_object (&fixture->device);
  g_clear_object (&fixture->settings);

  if (fixture->endpoint)
    {
      valent_channel_close (fixture->endpoint, NULL, NULL);
      v_await_finalize_object (fixture->endpoint);
    }

  if (fixture->channel)
    {
      valent_channel_close (fixture->channel, NULL, NULL);
      v_await_finalize_object (fixture->channel);
    }

  if (fixture->data && fixture->data_free)
    g_clear_pointer (&fixture->data, fixture->data_free);

  valent_test_await_pending ();
}

/**
 * valent_test_fixture_new:
 * @path: a file path
 *
 * Create a new `ValentTestFixture` for the JSON test data at @path.
 *
 * Returns: (transfer full): a new `ValentTestFixture`
 */
ValentTestFixture *
valent_test_fixture_new (const char *path)
{
  ValentTestFixture *fixture;

  g_assert (path != NULL && *path != '\0');

  fixture = g_rc_box_new0 (ValentTestFixture);
  valent_test_fixture_init (fixture, path);

  return g_steal_pointer (&fixture);
}

/**
 * valent_test_fixture_ref:
 * @fixture: a `ValentTestFixture`
 *
 * Acquire a new reference of @fixture.
 *
 * Returns: (transfer full): a `ValentTestFixture`
 */
ValentTestFixture *
valent_test_fixture_ref (ValentTestFixture *fixture)
{
  g_assert (fixture != NULL);

  return g_rc_box_acquire (fixture);
}

/**
 * valent_test_fixture_unref:
 * @fixture: a `ValentTestFixture`
 *
 * Release a reference on @fixture.
 */
void
valent_test_fixture_unref (ValentTestFixture *fixture)
{
  g_assert (fixture != NULL);

  g_rc_box_release_full (fixture, valent_test_fixture_free);
}

/**
 * valent_test_fixture_get_data:
 * @fixture: a `ValentTestFixture`
 *
 * Get the arbitrary data for @fixture.
 *
 * Returns: (transfer none): a data pointer
 */
gpointer
valent_test_fixture_get_data (ValentTestFixture *fixture)
{
  g_assert (fixture != NULL);

  return fixture->data;
}

/**
 * valent_test_fixture_set_data:
 * @fixture: a `ValentTestFixture`
 * @data: arbitrary data pointer
 * @data_free: a `GDestroyNotify`
 *
 * Set the arbitrary data for @fixture.
 */
void
valent_test_fixture_set_data (ValentTestFixture *fixture,
                              gpointer           data,
                              GDestroyNotify     data_free)
{
  g_assert (fixture != NULL);

  if (fixture->data && fixture->data_free)
    g_clear_pointer (&fixture->data, fixture->data_free);

  fixture->data = data;
  fixture->data_free = data_free;
}

/**
 * valent_test_fixture_get_device:
 * @fixture: a `ValentTestFixture`
 *
 * Get the `ValentDevice`.
 *
 * Returns: (transfer none): a `ValentDevice`
 */
ValentDevice *
valent_test_fixture_get_device (ValentTestFixture *fixture)
{
  g_assert (fixture != NULL);

  return fixture->device;
}

/**
 * valent_test_fixture_connect:
 * @fixture: a `ValentTestFixture`
 * @connected: whether to connect the device
 *
 * Get the connected state of the `ValentDevice`.
 */
void
valent_test_fixture_connect (ValentTestFixture *fixture,
                             gboolean           connect)
{
  g_assert (fixture != NULL);

  valent_device_set_channel (fixture->device, connect ? fixture->channel : NULL);
}

/**
 * valent_test_fixture_lookup_packet:
 * @fixture: a `ValentTestFixture`
 * @name: a name
 *
 * Lookup the test packet @name.
 *
 * Returns: (transfer none): a `JsonNode`
 */
JsonNode *
valent_test_fixture_lookup_packet (ValentTestFixture *fixture,
                                   const char        *name)
{
  g_assert (fixture != NULL);
  g_assert (name != NULL);

  return json_object_get_member (json_node_get_object (fixture->packets), name);
}

/**
 * valent_test_fixture_expect_packet:
 * @fixture: a `ValentTestFixture`
 *
 * Iterate the main context until a packet is received from the mock
 * `ValentDevice`.
 *
 * Returns: (transfer full): a `JsonNode`
 */
JsonNode *
valent_test_fixture_expect_packet (ValentTestFixture *fixture)
{
  JsonNode *packet = NULL;

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)expect_packet_cb,
                              &packet);

  while (packet == NULL)
    g_main_context_iteration (NULL, FALSE);

  return g_steal_pointer (&packet);
}

static void
valent_channel_write_packet_cb (GObject      *object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  gboolean *done = (gboolean *)user_data;
  GError *error = NULL;

  valent_channel_write_packet_finish (VALENT_CHANNEL (object), result, &error);
  g_assert_no_error (error);

  *done = TRUE;
}

/**
 * valent_test_fixture_handle_packet:
 * @fixture: a `ValentTestFixture`
 * @packet: a `JsonNode`
 *
 * Simulate sending @packet to the `ValentDevice` for @fixture.
 */
void
valent_test_fixture_handle_packet (ValentTestFixture *fixture,
                                   JsonNode          *packet)
{
  gboolean done = FALSE;

  g_assert (fixture != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  valent_channel_write_packet (fixture->endpoint,
                               packet,
                               NULL,
                               valent_channel_write_packet_cb,
                               &done);
  valent_test_await_boolean (&done);
}

/**
 * valent_test_fixture_download:
 * @fixture: a `ValentTestFixture`
 * @packet: a `JsonNode`
 * @error: (nullable): a `GError`
 *
 * Simulate downloading the transfer described by @packet from the `ValentDevice`
 * for @fixture.
 *
 * Returns: %TRUE if successful
 */
gboolean
valent_test_fixture_download (ValentTestFixture  *fixture,
                              JsonNode           *packet,
                              GError            **error)
{
  g_assert (fixture != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  return valent_test_download (fixture->endpoint, packet, error);
}

/**
 * valent_test_fixture_upload:
 * @fixture: a `ValentTestFixture`
 * @packet: a `JsonNode`
 * @file: a `GFile`
 * @error: (nullable): a `GError`
 *
 * Simulate uploading @file to the `ValentDevice` for @fixture.
 */
gboolean
valent_test_fixture_upload (ValentTestFixture  *fixture,
                            JsonNode           *packet,
                            GFile              *file,
                            GError            **error)
{
  g_assert (fixture != NULL);
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (G_IS_FILE (file));
  g_assert (error == NULL || *error == NULL);

  return valent_test_upload (fixture->endpoint, packet, file, error);
}

/**
 * valent_test_fixture_schema_fuzz:
 * @fixture: a `ValentTestFixture`
 * @path: (type filename): path to a JSON Schema
 *
 * Generate test vectors for the JSON Schema at @path and pass them to the
 * `ValentDevice` for @fixture.
 */
void
valent_test_fixture_schema_fuzz (ValentTestFixture *fixture,
                                 const char        *path)
{
#ifdef HAVE_WALBOTTLE
  g_autoptr (JsonParser) parser = NULL;
  g_autoptr (GBytes) data = NULL;
  WblSchema *schema;
  GPtrArray *instances;

  data = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  schema = wbl_schema_new ();
  wbl_schema_load_from_data (schema,
                             g_bytes_get_data (data, NULL),
                             g_bytes_get_size (data),
                             NULL);
  instances = wbl_schema_generate_instances (schema,
                                             WBL_GENERATE_INSTANCE_NONE);

  parser = json_parser_new ();

  for (unsigned int i = 0; i < instances->len; i++)
    {
      WblGeneratedInstance *instance = g_ptr_array_index (instances, i);
      const char *json = wbl_generated_instance_get_json (instance);
      JsonNode *packet;

      json_parser_load_from_data (parser, json, -1, NULL);
      packet = json_parser_get_root (parser);

      // Only test valid KDE Connect packets
      if (VALENT_IS_PACKET (packet))
        valent_test_fixture_handle_packet (fixture, packet);
    }

  g_ptr_array_unref (instances);
  g_object_unref (schema);
#endif /* HAVE_WALBOTTLE */
}

