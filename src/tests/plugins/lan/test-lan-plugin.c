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
  GSocketListener      *listener;
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

  v_assert_finalize_object (fixture->service);
  g_assert_finalize_object (fixture->channel);

  g_assert_finalize_object (fixture->endpoint);
  g_assert_finalize_object (fixture->certificate);
  g_assert_finalize_object (fixture->socket);

  if (fixture->listener != NULL)
    g_assert_finalize_object (fixture->listener);
  g_clear_pointer (&fixture->packets, json_node_unref);
}

static void
accept_cb (GSocketListener   *listener,
           GAsyncResult      *result,
           LanBackendFixture *fixture)
{
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  JsonNode *identity;
  const char *device_id;
  g_autoptr (GIOStream) tls_stream = NULL;

  connection = g_socket_listener_accept_finish (listener, result, NULL, NULL);

  /* The incoming TCP connection is in response to an outgoing UDP packet, so
   * the peer must now write its identity packet */
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream ((GIOStream *)connection),
                                             NULL, NULL);
  g_assert_nonnull (peer_identity);

  /* Now that we have the device ID we can authorize or reject certificates.
   * NOTE: We're the client when accepting incoming connections */
  device_id = valent_identity_get_device_id (peer_identity);
  tls_stream = valent_lan_encrypt_new_client (connection,
                                              fixture->certificate,
                                              device_id,
                                              NULL,
                                              NULL);
  g_assert_nonnull (tls_stream);

  /* Create the new channel */
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
accept_connection (LanBackendFixture *fixture)
{
  GError *error = NULL;
  g_autoptr (GSocketListener) listener = NULL;

  listener = g_socket_listener_new ();

  if (!g_socket_listener_add_inet_port (listener, ENDPOINT_PORT, NULL, &error))
    g_assert_no_error (error);

  g_socket_listener_accept_async (listener,
                                  NULL,
                                  (GAsyncReadyCallback)accept_cb,
                                  fixture);
}

static void
start_cb (ValentChannelService *service,
          GAsyncResult         *result,
          LanBackendFixture    *fixture)
{
  g_autoptr (GError) error = NULL;

  valent_channel_service_start_finish (service, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->loop);
}

static void
on_broadcast (GDataInputStream  *stream,
              GAsyncResult      *result,
              LanBackendFixture *fixture)
{
  g_autofree char *line = NULL;
  GError *error = NULL;

  line = g_data_input_stream_read_line_finish_utf8 (stream, result, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (line);

  fixture->data = valent_packet_deserialize (line, &error);
  g_assert_no_error (error);
  g_assert_nonnull (fixture->data);

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
connect_to_host_cb (GSocketClient     *client,
                    GAsyncResult      *result,
                    LanBackendFixture *fixture)
{
  g_autoptr (GSocketConnection) connection = NULL;
  GOutputStream *output_stream;
  g_autoptr (GIOStream) tls_stream = NULL;
  GError *error = NULL;
  JsonNode *identity;

  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");

  connection = g_socket_client_connect_to_host_finish (client, result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (connection);

  /* Write the local identity. Once we do this, both peers will have the ability
   * to authenticate or reject TLS certificates.
   */
  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));
  valent_packet_to_stream (output_stream, identity, NULL, &error);
  g_assert_no_error (error);

  /* We're the TLS Server when responding to identity broadcasts */
  tls_stream = valent_lan_encrypt_new_server (connection,
                                              fixture->certificate,
                                              "test-device",
                                              NULL,
                                              &error);
  g_assert_no_error (error);

  /* Create new channel */
  fixture->endpoint = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                                    "base-stream",   tls_stream,
                                    "certificate",   fixture->certificate,
                                    "host",          SERVICE_HOST,
                                    "port",          SERVICE_PORT,
                                    "identity",      identity,
                                    "peer-identity", fixture->data,
                                    NULL);
  g_clear_pointer (&fixture->data, json_node_unref);
}

static void
test_lan_service_incoming_broadcast (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  GError *error = NULL;
  g_autoptr (GSocketAddress) address = NULL;
  JsonNode *identity;
  g_autofree char *identity_json = NULL;

  /* Start the service */
  valent_channel_service_start (fixture->service,
                                NULL,
                                (GAsyncReadyCallback)start_cb,
                                fixture);
  g_main_loop_run (fixture->loop);

  /* Wait for the TCP connection */
  accept_connection (fixture);

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

  valent_channel_service_stop (fixture->service);
}

static void
test_lan_service_outgoing_broadcast (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  g_autoptr (GInputStream) unix_stream = NULL;
  g_autoptr (GDataInputStream) data_stream = NULL;
  g_autoptr (GSocketClient) client = NULL;

  /* Start the service */
  valent_channel_service_start (fixture->service,
                                NULL,
                                (GAsyncReadyCallback)start_cb,
                                fixture);
  g_main_loop_run (fixture->loop);

  /* Identify the service to the mock endpoint */
  valent_channel_service_identify (fixture->service, ENDPOINT_ADDR);

  unix_stream = g_unix_input_stream_new (g_socket_get_fd (fixture->socket), FALSE);
  data_stream = g_data_input_stream_new (unix_stream);
  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       (GAsyncReadyCallback)on_broadcast,
                                       fixture);
  g_main_loop_run (fixture->loop);

  /* Open a TCP connection */
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);

  g_socket_client_connect_to_host_async (client,
                                         SERVICE_ADDR,
                                         SERVICE_PORT,
                                         NULL,
                                         (GAsyncReadyCallback)connect_to_host_cb,
                                         fixture);

  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  g_main_loop_run (fixture->loop);

  valent_channel_service_stop (fixture->service);
}

static void
accept_upload_cb (ValentChannel *channel,
                  GAsyncResult  *result,
                  gpointer       user_data)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GOutputStream) target = NULL;
  gssize size, transferred;
  GError *error = NULL;

  packet = valent_channel_read_packet_finish (channel, result, &error);
  g_assert_no_error (error);

  size = valent_packet_get_payload_size (packet);
  g_assert_cmpint (size, >, -1);

  stream = valent_channel_download (channel, packet, NULL, &error);
  g_assert_no_error (error);

  target = g_memory_output_stream_new_resizable ();
  transferred = g_output_stream_splice (target,
                                        g_io_stream_get_input_stream (stream),
                                        (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                         G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                        NULL,
                                        &error);

  g_assert_no_error (error);
  g_assert_cmpint (size, ==, transferred);
}

static void
test_lan_service_channel (LanBackendFixture *fixture,
                          gconstpointer      user_data)
{
  GError *error = NULL;
  JsonNode *packet;
  g_autoptr (GSocketAddress) address = NULL;
  g_autofree char *identity_str = NULL;
  const char *verification_key;
  char *host;
  GTlsCertificate *certificate, *peer_certificate, *cert_cmp;
  guint16 port;
  g_autoptr (GFile) file = NULL;

  /* Start the service */
  valent_channel_service_start (fixture->service,
                                NULL,
                                (GAsyncReadyCallback)start_cb,
                                fixture);
  g_main_loop_run (fixture->loop);

  /* Wait for the TCP connection */
  accept_connection (fixture);

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
  verification_key = valent_channel_get_verification_key (fixture->channel);
  g_assert_nonnull (verification_key);

  g_object_get (fixture->channel,
                "certificate",      &certificate,
                "peer-certificate", &peer_certificate,
                "host",             &host,
                "port",             &port,
                NULL);

  cert_cmp = valent_lan_channel_get_certificate (VALENT_LAN_CHANNEL (fixture->endpoint));
  peer_certificate = valent_lan_channel_get_peer_certificate (VALENT_LAN_CHANNEL (fixture->channel));
  g_assert_true (g_tls_certificate_is_same (cert_cmp, peer_certificate));
  //g_object_unref (peer_certificate);

  cert_cmp = valent_lan_channel_get_peer_certificate (VALENT_LAN_CHANNEL (fixture->endpoint));
  g_assert_true (g_tls_certificate_is_same (cert_cmp, certificate));
  g_object_unref (certificate);

  g_assert_cmpstr (host, ==, ENDPOINT_HOST);
  g_assert_cmpuint (port, ==, ENDPOINT_PORT);
  g_free (host);

  /* Transfers */
  file = g_file_new_for_path (TEST_DATA_DIR"image.png");
  packet = json_object_get_member (json_node_get_object (fixture->packets),
                                   "transfer");

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)accept_upload_cb,
                              NULL);
  valent_test_upload (fixture->channel, packet, file, &error);
  g_assert_no_error (error);


  valent_channel_service_stop (fixture->service);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

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
