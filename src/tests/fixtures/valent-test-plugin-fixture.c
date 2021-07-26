// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyfixturehtText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-core.h>
#include <libwalbottle/wbl-schema.h>

#include "valent-device-private.h"
#include "valent-test-plugin-fixture.h"
#include "valent-test-utils.h"


/**
 * SECTION:valenttestpluginfixture
 * @short_description: A fixture for testing plugins
 * @title: ValentTestPluginFixture
 * @stability: Unstable
 * @include: libvalent-test.h
 *
 * #ValentTestPluginFixture is a fixture for testing #ValentDevicePlugin
 * implementations that require a connected #ValentDevice.
 */

G_DEFINE_BOXED_TYPE (ValentTestPluginFixture, valent_test_plugin_fixture,
                                              valent_test_plugin_fixture_copy,
                                              valent_test_plugin_fixture_free)

static JsonNode *expected_packet = NULL;

static void
expect_packet_cb (ValentChannel           *channel,
                  GAsyncResult            *result,
                  ValentTestPluginFixture *fixture)
{
  g_autoptr (GError) error = NULL;

  expected_packet = valent_channel_read_packet_finish (channel, result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

/*
 * Transfer Helpers
 */
static void
valent_test_plugin_fixture_init_channel (ValentTestPluginFixture *fixture)
{
  g_autofree ValentChannel **channels = NULL;
  JsonNode *peer_identity;

  peer_identity = valent_test_plugin_fixture_lookup_packet (fixture, "identity");
  channels = valent_test_channels (peer_identity, peer_identity);

  fixture->channel = g_steal_pointer (&channels[0]);
  fixture->endpoint = g_steal_pointer(&channels[1]);
}

/**
 * valent_test_plugin_fixture_new:
 * @path: a path to JSON test data
 *
 * Create a new test fixture.
 *
 * Returns: (transfer full): a #ValentTestPluginFixture
 */
ValentTestPluginFixture *
valent_test_plugin_fixture_new (const char *path)
{
  ValentTestPluginFixture *fixture;

  fixture = g_new0 (ValentTestPluginFixture, 1);
  valent_test_plugin_fixture_init (fixture, path);

  return fixture;
}

void
valent_test_plugin_fixture_free (gpointer data)
{
  ValentTestPluginFixture *fixture = data;

  valent_test_plugin_fixture_clear (fixture, NULL);
  g_free (fixture);
}

ValentTestPluginFixture *
valent_test_plugin_fixture_copy (ValentTestPluginFixture *fixture)
{
  return fixture;
}

/**
 * valent_test_plugin_fixture_init:
 * @fixture: a #ValentTestPluginFixture
 * @user_data: a file path
 *
 * A fixture setup function.
 */
void
valent_test_plugin_fixture_init (ValentTestPluginFixture *fixture,
                                 gconstpointer            user_data)
{
  g_autoptr (JsonParser) parser = NULL;
  JsonNode *identity;

  fixture->loop = g_main_loop_new (NULL, FALSE);

  /* Load test packets */
  parser = json_parser_new ();
  json_parser_load_from_file (parser, user_data, NULL);
  fixture->packets = json_parser_steal_root (parser);

  /* Init device */
  fixture->device = valent_device_new ("test-device");
  valent_device_set_paired (fixture->device, TRUE);

  identity = valent_test_plugin_fixture_lookup_packet (fixture, "identity");
  valent_device_handle_packet (fixture->device, identity);

  valent_test_plugin_fixture_init_channel (fixture);
}

/**
 * valent_test_plugin_fixture_init_settings:
 * @fixture: a #ValentTestPluginFixture
 * @name: a plugin module name
 *
 * Create a #GSettings object for the #ValentDevicePlugin module @name.
 */
void
valent_test_plugin_fixture_init_settings (ValentTestPluginFixture *fixture,
                                          const char              *name)
{
  const char *device_id;

  g_assert (fixture != NULL);
  g_assert (name != NULL);

  device_id = valent_device_get_id (fixture->device);
  fixture->settings = valent_device_plugin_new_settings (device_id, name);
}

void
valent_test_plugin_fixture_clear (ValentTestPluginFixture *fixture,
                                  gconstpointer            user_data)
{
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->packets, json_node_unref);
  g_clear_object (&fixture->device);
  g_clear_object (&fixture->settings);

  g_clear_object (&fixture->channel);
  g_clear_object (&fixture->endpoint);

  if (fixture->data && fixture->data_free)
    g_clear_pointer (&fixture->data, fixture->data_free);

  while (g_main_context_iteration (NULL, FALSE))
    continue;
}

/**
 * valent_test_plugin_fixture_get_data:
 * @fixture: a #ValentTestPluginFixture
 *
 * Get the arbitrary data for @fixture.
 *
 * Returns: (transfer none): a data pointer
 */
gpointer
valent_test_plugin_fixture_get_data (ValentTestPluginFixture *fixture)
{
  g_assert (fixture != NULL);

  return fixture->data;
}

/**
 * valent_test_plugin_fixture_set_data:
 * @fixture: a #ValentTestPluginFixture
 * @data: arbitrary data pointer
 * @data_free: a #GDestroyNotify
 *
 * Set the arbitrary data for @fixture.
 */
void
valent_test_plugin_fixture_set_data (ValentTestPluginFixture *fixture,
                                     gpointer                 data,
                                     GDestroyNotify           data_free)
{
  g_assert (fixture != NULL);

  if (fixture->data && fixture->data_free)
    g_clear_pointer (&fixture->data, fixture->data_free);

  fixture->data = data;
  fixture->data_free = data_free;
}

/**
 * valent_test_plugin_fixture_get_device:
 * @fixture: a #ValentTestPluginFixture
 *
 * Get the #ValentDevice.
 *
 * Returns: (transfer none): a #ValentDevice
 */
ValentDevice *
valent_test_plugin_fixture_get_device (ValentTestPluginFixture *fixture)
{
  g_assert (fixture != NULL);

  return fixture->device;
}

/**
 * valent_test_plugin_fixture_get_endpoint:
 * @fixture: a #ValentTestPluginFixture
 *
 * Get the endpoint #ValentChannel.
 *
 * Returns: (transfer none): a #ValentChannel
 */
ValentChannel *
valent_test_plugin_fixture_get_endpoint (ValentTestPluginFixture *fixture)
{
  g_assert (fixture != NULL);

  return fixture->endpoint;
}

/**
 * valent_test_plugin_fixture_run:
 * @fixture: a #ValentTestPluginFixture
 *
 * Start the #GMainLoop for @fixture.
 */
void
valent_test_plugin_fixture_run (ValentTestPluginFixture *fixture)
{
  g_assert (fixture != NULL);

  g_main_loop_run (fixture->loop);
}

/**
 * valent_test_plugin_fixture_quit:
 * @fixture: a #ValentTestPluginFixture
 *
 * Stop the #GMainLoop for @fixture.
 */
void
valent_test_plugin_fixture_quit (ValentTestPluginFixture *fixture)
{
  g_assert (fixture != NULL);

  g_main_loop_quit (fixture->loop);
}

/**
 * valent_test_plugin_fixture_connect:
 * @fixture: a #ValentTestPluginFixture
 * @connected: whether to connect the device
 *
 * Get the connected state of the #ValentDevice.
 */
void
valent_test_plugin_fixture_connect (ValentTestPluginFixture *fixture,
                                    gboolean                 connect)
{
  g_assert (fixture != NULL);

  valent_device_set_channel (fixture->device, connect ? fixture->channel : NULL);
}

/**
 * valent_test_plugin_fixture_lookup_packet:
 * @fixture: a #ValentTestPluginFixture
 * @name: a name
 *
 * Lookup the test packet @name.
 *
 * Returns: (transfer none): a #JsonNode
 */
JsonNode *
valent_test_plugin_fixture_lookup_packet (ValentTestPluginFixture *fixture,
                                          const char              *name)
{
  g_assert (fixture != NULL);
  g_assert (name != NULL);

  return json_object_get_member (json_node_get_object (fixture->packets), name);
}

/**
 * valent_test_plugin_fixture_expect_packet:
 * @fixture: a #ValentTestPluginFixture
 *
 * Synchronously read the next packet that has been sent by the #ValentDevice.
 *
 * Returns: (transfer full): a #JsonNode
 */
JsonNode *
valent_test_plugin_fixture_expect_packet (ValentTestPluginFixture *fixture)
{
  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)expect_packet_cb,
                              fixture);
  g_main_loop_run (fixture->loop);

  return g_steal_pointer (&expected_packet);
}

/**
 * valent_test_plugin_fixture_handle_packet:
 * @fixture: a #ValentTestPluginFixture
 * @packet: a #JsonNode
 *
 * Simulate sending @packet to the #ValentDevice for @fixture.
 */
void
valent_test_plugin_fixture_handle_packet (ValentTestPluginFixture *fixture,
                                          JsonNode                *packet)
{
  g_assert (fixture != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  valent_device_handle_packet (fixture->device, packet);
}

/**
 * valent_test_plugin_fixture_download:
 * @fixture: a #ValentTestPluginFixture
 * @packet: a #JsonNode
 * @error: (nullable): a #GError
 *
 * Simulate downloading the transfer described by @packet from the #ValentDevice
 * for @fixture.
 *
 * Returns: %TRUE if successful
 */
gboolean
valent_test_plugin_fixture_download (ValentTestPluginFixture  *fixture,
                                     JsonNode                 *packet,
                                     GError                  **error)
{
  g_assert (fixture != NULL);
  g_assert (VALENT_IS_PACKET (packet));

  return valent_test_download (fixture->endpoint, packet, error);
}

/**
 * valent_test_plugin_fixture_upload:
 * @fixture: a #ValentTestPluginFixture
 * @packet: a #JsonNode
 * @file: a #GFile
 * @error: (nullable): a #GError
 *
 * Simulate uploading @file to the #ValentDevice for @fixture.
 */
gboolean
valent_test_plugin_fixture_upload (ValentTestPluginFixture  *fixture,
                                   JsonNode                 *packet,
                                   GFile                    *file,
                                   GError                  **error)
{
  g_assert (fixture != NULL);
  g_assert (VALENT_IS_PACKET (packet));
  g_assert (G_IS_FILE (file));
  g_assert (error == NULL || *error == NULL);

  return valent_test_upload (fixture->endpoint, packet, file, error);
}

/**
 * valent_test_plugin_fixture_schema_fuzz:
 * @fixture: a #ValentTestPluginFixture
 * @path: (type filename): path to a JSON Schema
 *
 * Generate test vectors for the JSON Schema at @path and pass them to the
 * #ValentDevice for @fixture.
 */
gboolean
valent_test_plugin_fixture_schema_fuzz (ValentTestPluginFixture *fixture,
                                        const char              *path)
{
  g_autoptr (JsonParser) parser = NULL;
  WblSchema *schema;
  GPtrArray *instances;

  schema = wbl_schema_new ();
  wbl_schema_load_from_file (schema, path, NULL);
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
        valent_test_plugin_fixture_handle_packet (fixture, packet);
    }

  g_ptr_array_unref (instances);
  g_object_unref (schema);

  return TRUE;
}

