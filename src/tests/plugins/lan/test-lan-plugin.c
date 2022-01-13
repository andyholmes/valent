// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <libvalent-core.h>
#include <libvalent-test.h>

#include "valent-lan-utils.h"
#include "valent-lan-channel.h"
#include "valent-lan-channel-service.h"

#define ENDPOINT_ADDR "127.0.0.1:3716"
#define ENDPOINT_HOST "127.0.0.1"
#define ENDPOINT_PORT 3716
#define SERVICE_ADDR  "127.0.0.1:2716"
#define SERVICE_HOST  "127.0.0.1"
#define SERVICE_PORT  2716

typedef struct
{
  GMainLoop            *loop;
  JsonNode             *packets;

  ValentChannelService *service;
  ValentChannel        *channel;

  /* Endpoint */
  GTlsCertificate      *certificate;
  ValentChannel        *endpoint;
  GSocket              *socket;

  gpointer              data;
} LanBackendFixture;

static GSocket *
create_socket (void)
{
  GError *error = NULL;
  GSocket *socket;
  g_autoptr (GInetAddress) iaddr = NULL;
  g_autoptr (GSocketAddress) saddr = NULL;

  socket = g_socket_new (G_SOCKET_FAMILY_IPV6,
                         G_SOCKET_TYPE_DATAGRAM,
                         G_SOCKET_PROTOCOL_UDP,
                         &error);
  g_assert_no_error (error);

  iaddr = g_inet_address_new_any (G_SOCKET_FAMILY_IPV6);
  saddr = g_inet_socket_address_new (iaddr, ENDPOINT_PORT);

  if (g_socket_bind (socket, saddr, TRUE, &error))
    g_socket_set_broadcast (socket, TRUE);

  g_assert_no_error (error);
  g_assert_true (g_socket_speaks_ipv4 (socket));

  return socket;
}

static void
lan_service_fixture_set_up (LanBackendFixture *fixture,
                            gconstpointer      user_data)
{
  PeasPluginInfo *plugin_info;
  g_autofree char *path = NULL;
  GError *error = NULL;

  fixture->loop = g_main_loop_new (NULL, FALSE);

  plugin_info = peas_engine_get_plugin_info (valent_get_engine (), "lan");
  fixture->service = g_object_new (VALENT_TYPE_LAN_CHANNEL_SERVICE,
                                   "id",                "test-device",
                                   "broadcast-address", "127.0.0.255",
                                   "port",              SERVICE_PORT,
                                   "plugin-info",       plugin_info,
                                   NULL);

  fixture->packets = valent_test_load_json (TEST_DATA_DIR"/plugin-lan.json");
  fixture->socket = create_socket ();

  /* Generate a certificate for the phony client */
  path = g_dir_make_tmp ("XXXXXX.valent", NULL);
  fixture->certificate = valent_certificate_new_sync (path, &error);
  g_assert_no_error (error);
}

static void
lan_service_fixture_tear_down (LanBackendFixture *fixture,
                               gconstpointer      user_data)
{
  while (g_main_context_iteration (NULL, FALSE))
    continue;

  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->packets, json_node_unref);

  v_assert_finalize_object (fixture->service);
  g_assert_finalize_object (fixture->channel);

  g_assert_finalize_object (fixture->endpoint);
  g_assert_finalize_object (fixture->certificate);
  g_assert_finalize_object (fixture->socket);
}

/*
 * Endpoint Service
 */
static void
g_socket_listener_accept_cb (GSocketListener   *listener,
                             GAsyncResult      *result,
                             LanBackendFixture *fixture)
{
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  JsonNode *identity;
  const char *device_id;
  g_autoptr (GIOStream) tls_stream = NULL;
  GError *error = NULL;

  connection = g_socket_listener_accept_finish (listener, result, NULL, NULL);

  /* The incoming TCP connection is in response to the mock UDP packet we sent,
   * so we now expect the test service to write its identity packet.
   */
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                             NULL,
                                             &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (peer_identity));

  /* The test service is unverified, so we expect it to be accepted on a
   * trust-on-first-use basis.
   */
  device_id = valent_identity_get_device_id (peer_identity);
  tls_stream = valent_lan_encrypt_new_client (connection,
                                              fixture->certificate,
                                              device_id,
                                              NULL,
                                              &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CONNECTION (tls_stream));

  /* We're pretending to be a remote service, so we create an endpoint channel
   * so that we can pop packets of it from the test service.
   */
  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");
  fixture->endpoint = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                                    "base-stream",   tls_stream,
                                    "certificate",   fixture->certificate,
                                    "host",          SERVICE_HOST,
                                    "identity",      identity,
                                    "peer-identity", peer_identity,
                                    "port",          SERVICE_PORT,
                                    NULL);

  g_socket_listener_close (listener);
}

static void
await_incoming_connection (LanBackendFixture *fixture)
{
  GError *error = NULL;
  g_autoptr (GSocketListener) listener = NULL;

  listener = g_socket_listener_new ();

  if (!g_socket_listener_add_inet_port (listener, ENDPOINT_PORT, NULL, &error))
    g_assert_no_error (error);

  g_socket_listener_accept_async (listener,
                                  NULL,
                                  (GAsyncReadyCallback)g_socket_listener_accept_cb,
                                  fixture);
}

static void
g_socket_client_connect_to_host_cb (GSocketClient     *client,
                                    GAsyncResult      *result,
                                    LanBackendFixture *fixture)
{
  g_autoptr (GSocketConnection) connection = NULL;
  JsonNode *identity;
  g_autoptr (JsonNode) peer_identity = NULL;
  GOutputStream *output_stream;
  g_autoptr (GIOStream) tls_stream = NULL;
  GError *error = NULL;

  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");

  connection = g_socket_client_connect_to_host_finish (client, result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (connection);

  /* We opened a TCP connection in response to the incoming UDP broadcast so the
   * test service now expects us to write our identity packet.
   */
  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));
  valent_packet_to_stream (output_stream, identity, NULL, &error);
  g_assert_no_error (error);

  /* The test service is unverified, so we expect it to be accepted on a
   * trust-on-first-use basis.
   */
  tls_stream = valent_lan_encrypt_new_server (connection,
                                              fixture->certificate,
                                              "test-device",
                                              NULL,
                                              &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CONNECTION (tls_stream));

  /* We're pretending to be a remote service, so we create an endpoint channel
   * so that we can pop packets of it from the test service.
   */
  peer_identity = g_steal_pointer (&fixture->data);
  fixture->endpoint = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                                    "base-stream",   tls_stream,
                                    "certificate",   fixture->certificate,
                                    "host",          SERVICE_HOST,
                                    "port",          SERVICE_PORT,
                                    "identity",      identity,
                                    "peer-identity", peer_identity,
                                    NULL);
}

static void
on_incoming_broadcast (GDataInputStream  *stream,
                       GAsyncResult      *result,
                       LanBackendFixture *fixture)
{
  g_autoptr (GSocketClient) client = NULL;
  g_autofree char *line = NULL;
  GError *error = NULL;

  line = g_data_input_stream_read_line_finish_utf8 (stream,
                                                    result,
                                                    NULL,
                                                    &error);
  g_assert_no_error (error);
  g_assert_nonnull (line);

  fixture->data = valent_packet_deserialize (line, &error);
  g_assert_no_error (error);
  g_assert_nonnull (fixture->data);

  /* We opened a TCP connection in response to the incoming UDP broadcast so the
   * test service now expects us to write our identity packet.
   */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);

  g_socket_client_connect_to_host_async (client,
                                         SERVICE_ADDR,
                                         SERVICE_PORT,
                                         NULL,
                                         (GAsyncReadyCallback)g_socket_client_connect_to_host_cb,
                                         fixture);
}

static void
on_incoming_transfer (ValentChannel *endpoint,
                      GAsyncResult  *result,
                      gpointer       user_data)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GOutputStream) target = NULL;
  goffset payload_size, transferred;
  GError *error = NULL;

  /* We expect the packet to be properly populated with payload information */
  packet = valent_channel_read_packet_finish (endpoint, result, &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (packet));
  g_assert_true (valent_packet_has_payload (packet));

  payload_size = valent_packet_get_payload_size (packet);
  g_assert_cmpint (payload_size, >, 0);

  /* We expect to be able to create a transfer stream from the packet */
  stream = valent_channel_download (endpoint, packet, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_IO_STREAM (stream));

  /* We expect to be able to transfer the full payload */
  target = g_memory_output_stream_new_resizable ();
  transferred = g_output_stream_splice (target,
                                        g_io_stream_get_input_stream (stream),
                                        (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                         G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                        NULL,
                                        &error);
  g_assert_no_error (error);
  g_assert_cmpint (transferred, ==, payload_size);
}

/*
 * Test Service Callbacks
 */
static void
start_cb (ValentChannelService *service,
          GAsyncResult         *result,
          LanBackendFixture    *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_channel_service_start_finish (service, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  g_main_loop_quit (fixture->loop);
}

static void
on_channel (ValentChannelService *service,
            ValentChannel        *channel,
            LanBackendFixture    *fixture)
{
  fixture->channel = g_object_ref (channel);
  g_main_loop_quit (fixture->loop);
}

static void
test_lan_service_incoming_broadcast (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  GError *error = NULL;
  g_autoptr (GSocketAddress) address = NULL;
  JsonNode *identity;
  g_autofree char *identity_json = NULL;

  valent_channel_service_start (fixture->service,
                                NULL,
                                (GAsyncReadyCallback)start_cb,
                                fixture);
  g_main_loop_run (fixture->loop);

  /* Listen for an incoming TCP connection */
  await_incoming_connection (fixture);

  /* Identify the mock endpoint to the service */
  address = g_inet_socket_address_new_from_string (SERVICE_HOST, SERVICE_PORT);
  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");
  identity_json = valent_packet_serialize (identity);

  g_socket_send_to (fixture->socket,
                    address,
                    identity_json,
                    strlen (identity_json),
                    NULL,
                    &error);
  g_assert_no_error (error);

  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  g_main_loop_run (fixture->loop);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_channel_service_stop (fixture->service);
}

static void
test_lan_service_outgoing_broadcast (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  g_autoptr (GInputStream) unix_stream = NULL;
  g_autoptr (GDataInputStream) data_stream = NULL;

  valent_channel_service_start (fixture->service,
                                NULL,
                                (GAsyncReadyCallback)start_cb,
                                fixture);
  g_main_loop_run (fixture->loop);

  /* Send a UDP broadcast directly to the mock endpoint. When the identity
   * packet is received, the mock endpoint will respond by opening a TCP
   * connection to the test service.
   */
  valent_channel_service_identify (fixture->service, ENDPOINT_ADDR);

  unix_stream = g_unix_input_stream_new (g_socket_get_fd (fixture->socket), TRUE);
  data_stream = g_data_input_stream_new (unix_stream);
  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       (GAsyncReadyCallback)on_incoming_broadcast,
                                       fixture);

  /* When the test service accepts the incoming connection, it should negotiate
   * the TLS connection and create a channel.
   */
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  g_main_loop_run (fixture->loop);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_channel_service_stop (fixture->service);
}

static void
test_lan_service_channel (LanBackendFixture *fixture,
                          gconstpointer      user_data)
{
  GError *error = NULL;
  JsonNode *packet;
  g_autoptr (GSocketAddress) address = NULL;
  g_autofree char *identity_str = NULL;
  const char *channel_verification;
  const char *endpoint_verification;
  char *host;
  GTlsCertificate *certificate, *peer_certificate, *cert_cmp;
  guint16 port;
  g_autoptr (GFile) file = NULL;

  valent_channel_service_start (fixture->service,
                                NULL,
                                (GAsyncReadyCallback)start_cb,
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

  /* Properties */
  g_object_get (fixture->channel,
                "certificate",      &certificate,
                "peer-certificate", &peer_certificate,
                "host",             &host,
                "port",             &port,
                NULL);

  cert_cmp = valent_lan_channel_ref_certificate (VALENT_LAN_CHANNEL (fixture->endpoint));
  peer_certificate = valent_lan_channel_ref_peer_certificate (VALENT_LAN_CHANNEL (fixture->channel));
  g_assert_true (g_tls_certificate_is_same (cert_cmp, peer_certificate));
  g_clear_object (&cert_cmp);
  g_clear_object (&peer_certificate);

  cert_cmp = valent_lan_channel_ref_peer_certificate (VALENT_LAN_CHANNEL (fixture->endpoint));
  g_assert_true (g_tls_certificate_is_same (cert_cmp, certificate));
  g_clear_object (&cert_cmp);
  g_clear_object (&certificate);

  g_assert_cmpstr (host, ==, ENDPOINT_HOST);
  g_assert_cmpuint (port, ==, ENDPOINT_PORT);
  g_free (host);

  channel_verification = valent_channel_get_verification_key (fixture->channel);
  endpoint_verification = valent_channel_get_verification_key (fixture->endpoint);
  g_assert_nonnull (channel_verification);
  g_assert_nonnull (endpoint_verification);
  g_assert_cmpstr (channel_verification, ==, endpoint_verification);

  /* Transfers */
  file = g_file_new_for_path (TEST_DATA_DIR"image.png");
  packet = json_object_get_member (json_node_get_object (fixture->packets),
                                   "transfer");

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)on_incoming_transfer,
                              NULL);
  valent_test_upload (fixture->channel, packet, file, &error);
  g_assert_no_error (error);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_channel_service_stop (fixture->service);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_type_ensure (VALENT_TYPE_LAN_CHANNEL);
  g_type_ensure (VALENT_TYPE_LAN_CHANNEL_SERVICE);

  g_test_add ("/backends/lan-backend/incoming-broadcast",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_incoming_broadcast,
              lan_service_fixture_tear_down);

  g_test_add ("/backends/lan-backend/outgoing-broadcast",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_outgoing_broadcast,
              lan_service_fixture_tear_down);

  g_test_add ("/backends/lan-backend/channel",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_channel,
              lan_service_fixture_tear_down);

  return g_test_run ();
}
