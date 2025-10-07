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
  ValentChannelService *service;
  ValentChannel        *channel;
  JsonNode             *packets;

  /* D-Bus */
  GDBusConnection      *connection;
  int                  fds[2];

  /* Endpoint */
  ValentChannel        *endpoint;
  GTlsCertificate      *peer_certificate;
  JsonNode             *peer_identity;

  gpointer              data;
} BluezTestFixture;

static void
bluez_service_fixture_set_up (BluezTestFixture *fixture,
                              gconstpointer     user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autofree char *peer_certificate_pem = NULL;
  const char *peer_id = NULL;
  GError *error = NULL;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "bluez");
  context = valent_context_new (NULL, "plugin", "bluez");

  fixture->packets = valent_test_load_json ("plugin-bluez.json");
  fixture->peer_identity = json_object_get_member (json_node_get_object (fixture->packets),
                                                   "peer-identity");
  fixture->connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
  g_assert_no_errno (socketpair (AF_UNIX, SOCK_STREAM, 0, fixture->fds));

  /* Generate peer certificate and update the identity packet.
   */
  fixture->peer_certificate = valent_certificate_new_sync (NULL, &error);
  g_assert_no_error (error);

  peer_id = valent_certificate_get_common_name (fixture->peer_certificate);
  json_object_set_string_member (valent_packet_get_body (fixture->peer_identity),
                                 "deviceId", peer_id);

  g_object_get (fixture->peer_certificate,
                "certificate-pem", &peer_certificate_pem,
                NULL);
  json_object_set_string_member (valent_packet_get_body (fixture->peer_identity),
                                 "certificate", peer_certificate_pem);

  /* Prepare the local test service */
  fixture->service = g_object_new (VALENT_TYPE_BLUEZ_CHANNEL_SERVICE,
                                   "context",     context,
                                   "plugin-info", plugin_info,
                                   NULL);
}

static void
bluez_service_fixture_tear_down (BluezTestFixture *fixture,
                                 gconstpointer     user_data)
{
  g_clear_pointer (&fixture->packets, json_node_unref);
  g_clear_object (&fixture->connection);

  valent_object_destroy (VALENT_OBJECT (fixture->service));
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

  v_await_finalize_object (fixture->peer_certificate);
}

/*
 * Test Service Callbacks
 */
static void
g_async_initable_init_async_cb (GAsyncInitable   *initable,
                                GAsyncResult     *result,
                                BluezTestFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = g_async_initable_init_finish (initable, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  valent_test_quit_loop ();
}

static void
on_channel (ValentChannelService  *service,
            ValentChannel         *channel,
            ValentChannel        **channel_out)
{
  if (channel_out)
    *channel_out = g_object_ref (channel);
}

static void
dbusmock_update_uuids_cb (GDBusConnection  *connection,
                          GAsyncResult     *result,
                          BluezTestFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  valent_test_quit_loop ();
}

static void
dbusmock_pair_device_cb (GDBusConnection  *connection,
                         GAsyncResult     *result,
                         BluezTestFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GVariantBuilder props_builder;
  GVariant *props = NULL;
  GVariantBuilder uuids_builder;
  GVariant *uuids = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_finish (connection, result, &error);
  g_assert_no_error (error);

  /* Allow PropertiesChanged to be emitted
   */
  valent_test_await_pending ();

  /* Update the UUIDs to include KDE Connect's profile
   */
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
                          (GAsyncReadyCallback)dbusmock_update_uuids_cb,
                          fixture);
}

static void
dbusmock_setup_device (BluezTestFixture *fixture)
{
  /* Pair the device
   */
  g_dbus_connection_call (fixture->connection,
                          "org.bluez",
                          "/org/bluez",
                          "org.bluez.Mock",
                          "PairDevice",
                          g_variant_new ("(ss)",
                                         BLUEZ_ADAPTER_NAME,
                                         BLUEZ_DEVICE_ADDR),
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback)dbusmock_pair_device_cb,
                          fixture);
  valent_test_run_loop ();
}

static void
dbusmock_new_connection_cb (GDBusConnection  *connection,
                            GAsyncResult     *result,
                            BluezTestFixture *fixture)
{
  g_autoptr (GVariant) reply = NULL;
  GError *error = NULL;

  reply = g_dbus_connection_call_with_unix_fd_list_finish (connection,
                                                           NULL,
                                                           result,
                                                           &error);
  g_assert_no_error (error);
  valent_test_quit_loop ();
}

static void
dbusmock_new_connection (BluezTestFixture *fixture)
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
  valent_test_run_loop ();
}

static void
handshake_cb (ValentMuxConnection *connection,
              GAsyncResult        *result,
              BluezTestFixture    *fixture)
{
  GError *error = NULL;

  fixture->endpoint = valent_mux_connection_handshake_finish (connection,
                                                              result,
                                                              &error);
  g_assert_no_error (error);
}

static void
test_bluez_service_new_connection (BluezTestFixture *fixture,
                                   gconstpointer     user_data)
{
  g_autoptr (GSocket) socket = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (ValentMuxConnection) muxer = NULL;
  GError *error = NULL;

  VALENT_TEST_CHECK ("The service can be initialized");
  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               fixture);
  valent_test_run_loop ();

  /* Setup a new bluez device
   */
  dbusmock_setup_device (fixture);

  VALENT_TEST_CHECK ("The service announces itself to the network");
  valent_channel_service_identify (fixture->service, NULL);

  VALENT_TEST_CHECK ("The service accepts incoming connections");
  socket = g_socket_new_from_fd (fixture->fds[0], &error);
  g_assert_no_error (error);
  connection = g_object_new (G_TYPE_SOCKET_CONNECTION,
                             "socket", socket,
                             NULL);
  muxer = valent_mux_connection_new (G_IO_STREAM (connection));

  dbusmock_new_connection (fixture);
  valent_mux_connection_handshake (muxer,
                                   fixture->peer_identity,
                                   NULL,
                                   (GAsyncReadyCallback)handshake_cb,
                                   fixture);

  VALENT_TEST_CHECK ("The service creates channels for successful connections");
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    &fixture->channel);
  valent_test_await_pointer (&fixture->channel);
  valent_test_await_pointer (&fixture->endpoint);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
}

static void
test_bluez_service_channel (BluezTestFixture *fixture,
                            gconstpointer     user_data)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  g_autoptr (ValentMuxConnection) muxer = NULL;

  test_bluez_service_new_connection (fixture, user_data);
  valent_test_await_pending ();

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->channel,
                "muxer", &muxer,
                NULL);
  g_assert_true (VALENT_IS_MUX_CONNECTION (muxer));

  certificate = valent_channel_ref_certificate (fixture->endpoint);
  peer_certificate = valent_channel_ref_peer_certificate (fixture->channel);
  g_assert_true (g_tls_certificate_is_same (certificate, peer_certificate));
  g_clear_object (&certificate);
  g_clear_object (&peer_certificate);

  certificate = valent_channel_ref_certificate (fixture->channel);
  peer_certificate = valent_channel_ref_peer_certificate (fixture->endpoint);
  g_assert_true (g_tls_certificate_is_same (certificate, peer_certificate));
  g_clear_object (&certificate);
  g_clear_object (&peer_certificate);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_BLUEZ_CHANNEL);
  g_type_ensure (VALENT_TYPE_BLUEZ_CHANNEL_SERVICE);

  g_test_add ("/plugins/bluez/new-connection",
              BluezTestFixture, NULL,
              bluez_service_fixture_set_up,
              test_bluez_service_new_connection,
              bluez_service_fixture_tear_down);

  g_test_add ("/plugins/bluez/channel",
              BluezTestFixture, NULL,
              bluez_service_fixture_set_up,
              test_bluez_service_channel,
              bluez_service_fixture_tear_down);

  return g_test_run ();
}
