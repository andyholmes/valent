// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <gio/gunixinputstream.h>
#include <valent.h>
#include <libvalent-test.h>
#include <sys/socket.h>

#include "valent-bluez-channel.h"
#include "valent-bluez-channel-service.h"
#include "valent-bluez-profile.h"
#include "valent-mux-connection.h"

#define BLUEZ_ADAPTER_NAME "hci0"
#define BLUEZ_DEVICE_ADDR  "AA:BB:CC:DD:EE:FF"
#define BLUEZ_DEVICE_PATH  "/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF"


typedef struct
{
  GMainLoop            *loop;
  JsonNode             *packets;
  GDBusConnection      *connection;
  int                  *fds;

  ValentChannelService *service;
  ValentChannel        *channel;

  /* Endpoint */
  ValentChannel        *endpoint;

  gpointer              data;
} BluezBackendFixture;

static void
bluez_service_fixture_set_up (BluezBackendFixture *fixture,
                              gconstpointer        user_data)
{
  PeasPluginInfo *plugin_info;

  fixture->loop = g_main_loop_new (NULL, FALSE);
  fixture->packets = valent_test_load_json ("plugin-bluez.json");
  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);

  plugin_info = peas_engine_get_plugin_info (valent_get_plugin_engine (), "bluez");
  fixture->service = g_object_new (VALENT_TYPE_BLUEZ_CHANNEL_SERVICE,
                                   "plugin-info", plugin_info,
                                   NULL);

  fixture->fds = g_new0 (int, 2);
  g_assert_no_errno (socketpair (AF_UNIX, SOCK_STREAM, 0, fixture->fds));
}

static void
bluez_service_fixture_tear_down (BluezBackendFixture *fixture,
                                 gconstpointer        user_data)
{
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->packets, json_node_unref);
  g_clear_object (&fixture->connection);

  g_clear_pointer (&fixture->fds, g_free);

  v_await_finalize_object (fixture->service);

  if (fixture->channel)
    {
      valent_channel_close_async (fixture->channel, NULL, NULL, NULL);
      v_await_finalize_object (fixture->channel);
    }

  if (fixture->endpoint)
    {
      valent_channel_close_async (fixture->endpoint, NULL, NULL, NULL);
      v_await_finalize_object (fixture->endpoint);
    }
}

/*
 * Test Service Callbacks
 */
static void
g_async_initable_init_async_cb (GAsyncInitable      *initable,
                                GAsyncResult        *result,
                                BluezBackendFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = g_async_initable_init_finish (initable, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_main_loop_quit (fixture->loop);
}

static void
on_channel (ValentChannelService *service,
            ValentChannel        *channel,
            BluezBackendFixture  *fixture)
{
  fixture->channel = g_object_ref (channel);
  g_main_loop_quit (fixture->loop);
}

static void
dbusmock_call_wait_cb (GDBusConnection     *connection,
                       GAsyncResult        *result,
                       BluezBackendFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}


static void
dbusmock_pair_device (BluezBackendFixture *fixture)
{
  g_dbus_connection_call (fixture->connection,
                          "org.bluez",
                          "/org/bluez",
                          "org.bluez.Mock",
                          "PairDevice",
                          g_variant_new ("(ssi)",
                                         BLUEZ_ADAPTER_NAME,
                                         BLUEZ_DEVICE_ADDR,
                                         5898764),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)dbusmock_call_wait_cb,
                          fixture);
  g_main_loop_run (fixture->loop);
}

static void
dbusmock_update_uuids (BluezBackendFixture *fixture)
{
  GVariantBuilder props_builder;
  GVariant *props = NULL;
  GVariantBuilder uuids_builder;
  GVariant *uuids = NULL;

  g_variant_builder_init (&uuids_builder, G_VARIANT_TYPE_STRING_ARRAY);
  g_variant_builder_add (&uuids_builder, "s", "00001105-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "0000110a-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "0000110c-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "00001112-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "00001115-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "00001116-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "0000111f-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "0000112f-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", "00001200-0000-1000-8000-00805f9b34fb");
  g_variant_builder_add (&uuids_builder, "s", VALENT_BLUEZ_PROFILE_UUID);
  uuids = g_variant_builder_end (&uuids_builder);

  g_variant_builder_init (&props_builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&props_builder, "{sv}", "UUIDs", uuids);
  props = g_variant_builder_end (&props_builder);

  g_dbus_connection_call (fixture->connection,
                          "org.bluez",
                          BLUEZ_DEVICE_PATH,
                          "org.freedesktop.DBus.Mock",
                          "UpdateProperties",
                          g_variant_new ("(s@a{sv})", "org.bluez.Device1", props),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)dbusmock_call_wait_cb,
                          fixture);
  g_main_loop_run (fixture->loop);
}

static void
dbusmock_new_connection_cb (GDBusConnection     *connection,
                            GAsyncResult        *result,
                            BluezBackendFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_with_unix_fd_list_finish (connection,
                                                           NULL,
                                                           result,
                                                           &error);
  g_assert_no_error (error);
  g_main_loop_quit (fixture->loop);
}

static void
dbusmock_new_connection (BluezBackendFixture *fixture)
{
  const char *unique_name;
  GVariantBuilder fd_props_builder;
  GVariant *fd_props = NULL;
  GUnixFDList *fd_list = NULL;
  int fd_index;
  GError *error = NULL;

  unique_name = g_dbus_connection_get_unique_name (fixture->connection);

  g_variant_builder_init (&fd_props_builder, G_VARIANT_TYPE_VARDICT);
  fd_props = g_variant_builder_end (&fd_props_builder);

  fd_list = g_unix_fd_list_new ();
  fd_index = g_unix_fd_list_append (fd_list, fixture->fds[1], &error);
  g_assert_no_error (error);

  g_dbus_connection_call_with_unix_fd_list (fixture->connection,
                                            unique_name,
                                            VALENT_BLUEZ_PROFILE_PATH,
                                            "org.bluez.Profile1",
                                            "NewConnection",
                                            g_variant_new ("(oh@a{sv})",
                                                           BLUEZ_DEVICE_PATH,
                                                           fd_index,
                                                           fd_props),
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            fd_list,
                                            NULL,
                                            (GAsyncReadyCallback)dbusmock_new_connection_cb,
                                            fixture);
  g_object_unref (fd_list);
  g_main_loop_run (fixture->loop);
}

static void
handshake_cb (ValentMuxConnection *connection,
              GAsyncResult        *result,
              BluezBackendFixture *fixture)
{
  GError *error = NULL;

  fixture->endpoint = valent_mux_connection_handshake_finish (connection,
                                                              result,
                                                              &error);
  g_assert_no_error (error);
}

static void
test_bluez_service_new_connection (BluezBackendFixture *fixture,
                                   gconstpointer        user_data)
{
  g_autoptr (GSocket) socket = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (ValentMuxConnection) muxer = NULL;
  JsonNode *identity;
  GError *error = NULL;

  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               fixture);
  g_main_loop_run (fixture->loop);

  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);

  /* Pair and wait for PropertiesChanged to resolve */
  dbusmock_pair_device (fixture);
  valent_test_await_pending ();

  /* Update UUIDs and identify */
  dbusmock_update_uuids (fixture);
  valent_channel_service_identify (fixture->service, NULL);
  valent_test_await_pending ();

  /* Open connection */
  socket = g_socket_new_from_fd (fixture->fds[0], &error);
  g_assert_no_error (error);
  connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                             "socket", socket,
                             NULL);
  muxer = valent_mux_connection_new (G_IO_STREAM (connection));

  dbusmock_new_connection (fixture);
  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");
  valent_mux_connection_handshake (muxer,
                                   identity,
                                   NULL,
                                   (GAsyncReadyCallback)handshake_cb,
                                   fixture);
  g_main_loop_run (fixture->loop);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

#if 0
static void
test_bluez_service_channel (BluezBackendFixture *fixture,
                            gconstpointer        user_data)
{
  GError *error = NULL;
  JsonNode *packet;
  g_autoptr (GSocketAddress) address = NULL;
  g_autofree char *identity_str = NULL;
  g_autoptr (GFile) file = NULL;

  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               fixture);
  g_main_loop_run (fixture->loop);

  /* Listen for an incoming TCP connection */
  await_incoming_connection (fixture);

  /* Identify the mock endpoint to the service */
  address = g_inet_socket_address_new_from_string (SERVICE_HOST, SERVICE_PORT);
  packet = json_object_get_member (json_node_get_object (fixture->packets),
                                   "identity");
  identity_str = valent_packet_serialize (packet);

  g_socket_send_to (fixture->socket,
                    address,
                    identity_str,
                    strlen (identity_str),
                    NULL,
                    &error);
  g_assert_no_error (error);

  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  g_main_loop_run (fixture->loop);

  /* Transfers */
  file = g_file_new_for_uri ("resource:///tests/image.png");
  packet = json_object_get_member (json_node_get_object (fixture->packets),
                                   "transfer");

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)on_incoming_transfer,
                              NULL);
  valent_test_upload (fixture->channel, packet, file, &error);
  g_assert_no_error (error);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}
#endif

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_BLUEZ_CHANNEL);
  g_type_ensure (VALENT_TYPE_BLUEZ_CHANNEL_SERVICE);

  g_test_add ("/plugins/bluez/new-connection",
              BluezBackendFixture, NULL,
              bluez_service_fixture_set_up,
              test_bluez_service_new_connection,
              bluez_service_fixture_tear_down);

#if 0
  g_test_add ("/plugins/bluez/channel",
              BluezBackendFixture, NULL,
              bluez_service_fixture_set_up,
              test_bluez_service_channel,
              bluez_service_fixture_tear_down);
#endif

  return g_test_run ();
}
