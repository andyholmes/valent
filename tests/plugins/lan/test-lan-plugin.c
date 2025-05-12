// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-lan-utils.h"
#include "valent-lan-channel.h"
#include "valent-lan-channel-service.h"

#define HANDSHAKE_TIMEOUT_MS            (1100)
#define IDENTITY_BUFFER_MAX             (8192)

/* NOTE: These ports must be between 1716-1764 or they will trigger an error.
 *       Port 1716 is still avoided, since it would conflict with a running
 *       service when testing on a real system.
 */
#define ENDPOINT_ADDR                   "127.0.0.1:1717"
#define ENDPOINT_HOST                   "127.0.0.1"
#define ENDPOINT_PORT                   (1717)
#define SERVICE_ADDR                    "127.0.0.1:1718"
#define SERVICE_HOST                    "127.0.0.1"
#define SERVICE_PORT                    (1718)

#define TEST_INCOMING_IDENTITY_OVERSIZE "/plugins/lan/incoming-identity-oversize"
#define TEST_INCOMING_IDENTITY_TIMEOUT  "/plugins/lan/incoming-identity-timeout"
#define TEST_INCOMING_INVALID_ID        "/plugins/lan/incoming-invalid-id"
#define TEST_INCOMING_INVALID_NAME      "/plugins/lan/incoming-invalid-name"
#define TEST_OUTGOING_IDENTITY_OVERSIZE "/plugins/lan/outgoing-identity-oversize"
#define TEST_OUTGOING_IDENTITY_TIMEOUT  "/plugins/lan/outgoing-identity-timeout"
#define TEST_OUTGOING_INVALID_ID        "/plugins/lan/outgoing-invalid-id"
#define TEST_OUTGOING_INVALID_NAME      "/plugins/lan/outgoing-invalid-name"
#define TEST_INCOMING_TLS_CERTIFICATE   "/plugins/lan/incoming-tls-certificate"
#define TEST_OUTGOING_TLS_CERTIFICATE   "/plugins/lan/outgoing-tls-certificate"
#define TEST_INCOMING_TLS_IDENTITY      "/plugins/lan/incoming-tls-common-name"
#define TEST_OUTGOING_TLS_IDENTITY      "/plugins/lan/outgoing-tls-common-name"


typedef struct
{
  ValentChannelService *service;
  ValentChannel        *channel;
  JsonNode             *packets;

  /* Endpoint */
  ValentChannel        *endpoint;
  GTlsCertificate      *peer_certificate;
  JsonNode             *peer_identity;
  GSocket              *socket;

  gpointer              data;
} LanTestFixture;

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
lan_service_fixture_set_up (LanTestFixture *fixture,
                            gconstpointer   user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  const char *peer_id = NULL;
  GError *error = NULL;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "lan");
  context = valent_context_new (NULL, "plugin", "lan");

  fixture->packets = valent_test_load_json ("plugin-lan.json");
  fixture->peer_identity = json_object_get_member (json_node_get_object (fixture->packets),
                                                   "peer-identity");
  fixture->socket = create_socket ();

  /* Generate peer certificate and update the identity packet.
   */
  fixture->peer_certificate = valent_certificate_new_sync (NULL, &error);
  g_assert_no_error (error);

  peer_id = valent_certificate_get_common_name (fixture->peer_certificate);
  json_object_set_string_member (valent_packet_get_body (fixture->peer_identity),
                                 "deviceId", peer_id);

  /* Prepare the local test service */
  fixture->service = g_object_new (VALENT_TYPE_LAN_CHANNEL_SERVICE,
                                   "context",           context,
                                   "plugin-info",       plugin_info,
                                   "broadcast-address", "127.0.0.255",
                                   "port",              SERVICE_PORT,
                                   NULL);
}

static void
lan_service_fixture_tear_down (LanTestFixture *fixture,
                               gconstpointer   user_data)
{
  g_clear_pointer (&fixture->packets, json_node_unref);

  valent_object_destroy (VALENT_OBJECT (fixture->service));
  v_await_finalize_object (fixture->service);

  if (fixture->channel != NULL)
    v_await_finalize_object (fixture->channel);
  if (fixture->endpoint != NULL)
    v_await_finalize_object (fixture->endpoint);

  v_await_finalize_object (fixture->peer_certificate);
  v_await_finalize_object (fixture->socket);
}

/*
 * Test Service Callbacks
 */
static void
g_async_initable_init_async_cb (GAsyncInitable *initable,
                                GAsyncResult   *result,
                                gboolean       *done)
{
  GError *error = NULL;

  *done = g_async_initable_init_finish (initable, result, &error);
  g_assert_no_error (error);
}

static void
on_channel (ValentChannelService  *service,
            ValentChannel         *channel,
            ValentChannel        **channel_out)
{
  if (channel_out)
    *channel_out = g_object_ref (channel);
}

/*
 * Incoming Broadcast (Outgoing TCP Connection)
 */
static void
g_socket_listener_accept_cb (GSocketListener *listener,
                             GAsyncResult    *result,
                             LanTestFixture  *fixture)
{
  const char *test_name = g_test_get_path ();
  g_autoptr (GSocketConnection) connection = NULL;
  GTlsCertificate *peer_certificate = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;
  int64_t protocol_version = VALENT_NETWORK_PROTOCOL_MAX;
  GError *error = NULL;

  VALENT_TEST_CHECK ("The service opens outgoing connections");
  connection = g_socket_listener_accept_finish (listener, result, NULL, &error);
  g_socket_listener_close (listener);
  g_assert_no_error (error);

  /* In this test case we are neglecting to read the peer's identity packet, so
   * we expect the test service to close the connection after 1000ms.
   */
  if (g_strcmp0 (test_name, TEST_OUTGOING_IDENTITY_TIMEOUT) == 0)
    {
      VALENT_TEST_CHECK ("The service rejects connections that take too long "
                         "sending their identity");
      valent_test_await_timeout (HANDSHAKE_TIMEOUT_MS);
    }

  /* The incoming TCP connection is in response to the mock UDP packet we sent,
   * so we now expect the test service to write its identity packet.
   */
  VALENT_TEST_CHECK ("The service writes its identity after connecting");
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                             IDENTITY_BUFFER_MAX,
                                             NULL,
                                             &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (peer_identity));

  VALENT_TEST_CHECK ("The service uses a valid device ID");
  valent_packet_get_string (peer_identity, "deviceId", &device_id);
  g_assert_true (valent_device_validate_id (device_id));

  VALENT_TEST_CHECK ("The service negotiates TLS connections as the server");
  tls_stream = valent_lan_connection_handshake (connection,
                                                fixture->peer_certificate,
                                                NULL, /* trusted certificate */
                                                TRUE, /* is_client */
                                                NULL,
                                                &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CONNECTION (tls_stream));

  valent_packet_get_int (peer_identity, "protocolVersion", &protocol_version);
  if (protocol_version >= VALENT_NETWORK_PROTOCOL_V8)
    {
      g_autoptr (JsonNode) secure_identity = NULL;

      VALENT_TEST_CHECK ("The service exchanges identity packets over TLS");
      valent_packet_to_stream (g_io_stream_get_output_stream (tls_stream),
                               fixture->peer_identity,
                               NULL,
                               &error);
      g_assert_no_error (error);

      secure_identity = valent_packet_from_stream (g_io_stream_get_input_stream (tls_stream),
                                                   IDENTITY_BUFFER_MAX,
                                                   NULL,
                                                   &error);
      g_assert_no_error (error);

      g_clear_pointer (&peer_identity, json_node_unref);
      peer_identity = g_steal_pointer (&secure_identity);
    }

  /* We're pretending to be a remote service, so we create an endpoint channel
   * so that we can pop packets of it from the test service.
   */
  peer_certificate = g_tls_connection_get_peer_certificate (G_TLS_CONNECTION (tls_stream));
  fixture->endpoint = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                                    "base-stream",      tls_stream,
                                    "certificate",      fixture->peer_certificate,
                                    "identity",         fixture->peer_identity,
                                    "peer-certificate", peer_certificate,
                                    "peer-identity",    peer_identity,
                                    "host",             SERVICE_HOST,
                                    "port",             SERVICE_PORT,
                                    NULL);
}

static void
await_incoming_connection (LanTestFixture *fixture)
{
  g_autoptr (GSocketListener) listener = NULL;
  GError *error = NULL;

  listener = g_socket_listener_new ();
  if (!g_socket_listener_add_inet_port (listener, ENDPOINT_PORT, NULL, &error))
    g_assert_no_error (error);

  g_socket_listener_accept_async (listener,
                                  NULL,
                                  (GAsyncReadyCallback)g_socket_listener_accept_cb,
                                  fixture);
}

static void
test_lan_service_incoming_broadcast (LanTestFixture *fixture,
                                     gconstpointer   user_data)
{
  GError *error = NULL;
  g_autoptr (GSocketAddress) address = NULL;
  g_autofree char *identity_json = NULL;
  size_t identity_len;
  gboolean watch = FALSE;

  VALENT_TEST_CHECK ("The service can be initialized");
  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               &watch);
  valent_test_await_boolean (&watch);

  /* Listen for an incoming TCP connection */
  await_incoming_connection (fixture);

  VALENT_TEST_CHECK ("The service accepts UDP broadcasts from the network");
  address = g_inet_socket_address_new_from_string (SERVICE_HOST, SERVICE_PORT);
  identity_json = valent_packet_serialize (fixture->peer_identity, &identity_len);
  g_socket_send_to (fixture->socket,
                    address,
                    identity_json,
                    identity_len,
                    NULL,
                    &error);
  g_assert_no_error (error);

  VALENT_TEST_CHECK ("The service creates channels for successful connections");
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    &fixture->channel);
  valent_test_await_pointer (&fixture->channel);

  g_signal_handlers_disconnect_by_data (fixture->service, &fixture->channel);
}

/*
 * Outgoing Broadcast (Incoming Connection)
 */
static void
g_socket_client_connect_to_host_cb (GSocketClient      *client,
                                    GAsyncResult       *result,
                                    GSocketConnection **connection)
{
  GError *error = NULL;

  *connection = g_socket_client_connect_to_host_finish (client, result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (*connection);
}

static void
on_outgoing_broadcast (GDataInputStream  *stream,
                       GAsyncResult      *result,
                       JsonNode         **peer_identity)
{
  g_autofree char *line = NULL;
  GError *error = NULL;

  line = g_data_input_stream_read_line_finish_utf8 (stream,
                                                    result,
                                                    NULL,
                                                    &error);
  g_assert_no_error (error);
  g_assert_nonnull (line);

  *peer_identity = valent_packet_deserialize (line, &error);
  g_assert_no_error (error);
  g_assert_nonnull (*peer_identity);
}

static void
test_lan_service_outgoing_broadcast (LanTestFixture *fixture,
                                     gconstpointer   user_data)
{
  const char *test_name = g_test_get_path ();
  g_autoptr (GInputStream) unix_stream = NULL;
  g_autoptr (GDataInputStream) data_stream = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  GTlsCertificate *peer_certificate = NULL;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;
  int64_t protocol_version = VALENT_NETWORK_PROTOCOL_MAX;
  gboolean watch = FALSE;
  GError *error = NULL;

  VALENT_TEST_CHECK ("The service can be initialized");
  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               &watch);
  valent_test_await_boolean (&watch);

  VALENT_TEST_CHECK ("The service announces itself to the network");
  valent_channel_service_identify (fixture->service, ENDPOINT_ADDR);
  unix_stream = g_unix_input_stream_new (g_socket_get_fd (fixture->socket), TRUE);
  data_stream = g_data_input_stream_new (unix_stream);
  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       (GAsyncReadyCallback)on_outgoing_broadcast,
                                       &peer_identity);
  valent_test_await_pointer (&peer_identity);
  VALENT_JSON (peer_identity, g_get_host_name ());

  VALENT_TEST_CHECK ("The service accepts incoming connections");
  client = g_object_new (G_TYPE_SOCKET_CLIENT,
                         "enable-proxy", FALSE,
                         NULL);
  g_socket_client_connect_to_host_async (client,
                                         SERVICE_ADDR,
                                         SERVICE_PORT,
                                         NULL,
                                         (GAsyncReadyCallback)g_socket_client_connect_to_host_cb,
                                         &connection);
  valent_test_await_pointer (&connection);

  /* In this test case we are neglecting to send our identity packet, so
   * we expect the test service to close the connection after 1000ms
   */
  if (g_strcmp0 (test_name, TEST_INCOMING_IDENTITY_TIMEOUT) == 0)
    {
      VALENT_TEST_CHECK ("The service rejects connections that take too long "
                         "sending their identity");
      valent_test_await_timeout (HANDSHAKE_TIMEOUT_MS);
    }

  valent_packet_to_stream (g_io_stream_get_output_stream (G_IO_STREAM (connection)),
                           fixture->peer_identity,
                           NULL,
                           &error);
  g_assert_no_error (error);

  VALENT_TEST_CHECK ("The service negotiates TLS connections as the client");
  tls_stream = valent_lan_connection_handshake (connection,
                                                fixture->peer_certificate,
                                                NULL,  /* trusted certificate */
                                                FALSE, /* is_client */
                                                NULL,
                                                &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CONNECTION (tls_stream));

  valent_packet_get_int (peer_identity, "protocolVersion", &protocol_version);
  if (protocol_version >= VALENT_NETWORK_PROTOCOL_V8)
    {
      g_autoptr (JsonNode) secure_identity = NULL;

      VALENT_TEST_CHECK ("The service exchanges identity packets over TLS");
      valent_packet_to_stream (g_io_stream_get_output_stream (tls_stream),
                               fixture->peer_identity,
                               NULL,
                               &error);
      g_assert_no_error (error);

      secure_identity = valent_packet_from_stream (g_io_stream_get_input_stream (tls_stream),
                                                   IDENTITY_BUFFER_MAX,
                                                   NULL,
                                                   &error);
      g_assert_no_error (error);

      g_clear_pointer (&peer_identity, json_node_unref);
      peer_identity = g_steal_pointer (&secure_identity);
    }

  peer_certificate = g_tls_connection_get_peer_certificate (G_TLS_CONNECTION (tls_stream));
  fixture->endpoint = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                                    "base-stream",      tls_stream,
                                    "certificate",      fixture->peer_certificate,
                                    "identity",         fixture->peer_identity,
                                    "peer-certificate", peer_certificate,
                                    "peer-identity",    peer_identity,
                                    "host",             SERVICE_HOST,
                                    "port",             SERVICE_PORT,
                                    NULL);

  VALENT_TEST_CHECK ("The service creates channels for successful connections");
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    &fixture->channel);
  valent_test_await_pointer (&fixture->channel);

  g_signal_handlers_disconnect_by_data (fixture->service, &fixture->channel);
}

/*
 * Channel
 */
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

static void
test_lan_service_channel (LanTestFixture *fixture,
                          gconstpointer   user_data)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  g_autofree char *host = NULL;
  unsigned int port;
  g_autoptr (GFile) file = NULL;
  JsonNode *packet;
  GError *error = NULL;

  test_lan_service_incoming_broadcast (fixture, user_data);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->channel,
                "host", &host,
                "port", &port,
                NULL);

  g_assert_cmpstr (host, ==, ENDPOINT_HOST);
  g_assert_cmpuint (port, ==, ENDPOINT_PORT);

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

  VALENT_TEST_CHECK ("Channel can transfer payloads");
  file = g_file_new_for_uri ("resource:///tests/image.png");
  packet = json_object_get_member (json_node_get_object (fixture->packets),
                                   "transfer");

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)on_incoming_transfer,
                              NULL);
  valent_test_upload (fixture->channel, packet, file, &error);
  g_assert_no_error (error);
}

/*
 * Compliance Tests
 */
typedef void (*LanFixtureFunc) (LanTestFixture *fixture,
                                gconstpointer      user_data);

typedef struct
{
  const char     *name;
  const char     *errmsg;
  LanFixtureFunc  func;
} LanTestCase;

static LanTestCase compliance_tests[] = {
  {
    .name = TEST_OUTGOING_IDENTITY_OVERSIZE,
    .errmsg = "*unterminated string constant*",
    .func = (LanFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_INCOMING_IDENTITY_OVERSIZE,
    .errmsg = "*Packet too large*",
    .func = (LanFixtureFunc)test_lan_service_outgoing_broadcast,
  },
#if !(VALENT_SANITIZE_ADDRESS)
  {
    .name = TEST_OUTGOING_IDENTITY_TIMEOUT,
    .errmsg = "*timed out waiting for peer identity*",
    .func = (LanFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_INCOMING_IDENTITY_TIMEOUT,
    .errmsg = "*timed out waiting for peer identity*",
    .func = (LanFixtureFunc)test_lan_service_outgoing_broadcast,
  },
#endif /* !VALENT_SANITIZE_ADDRESS */
  {
    .name = TEST_INCOMING_INVALID_ID,
    .errmsg = "*invalid device ID*",
    .func = (LanFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_INVALID_ID,
    .errmsg = "*invalid device ID*",
    .func = (LanFixtureFunc)test_lan_service_outgoing_broadcast,
  },
  {
    .name = TEST_INCOMING_INVALID_NAME,
    .errmsg = "*invalid device name*",
    .func = (LanFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_INVALID_NAME,
    .errmsg = "*invalid device name*",
    .func = (LanFixtureFunc)test_lan_service_outgoing_broadcast,
  },
  {
    .name = TEST_INCOMING_TLS_CERTIFICATE,
    .errmsg = "*device ID does not match certificate common name*", // TODO
    .func = (LanFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_TLS_CERTIFICATE,
    .errmsg = "*device ID does not match certificate common name*", // TODO
    .func = (LanFixtureFunc)test_lan_service_outgoing_broadcast,
  },
  {
    .name = TEST_INCOMING_TLS_IDENTITY,
    .errmsg = "*device ID does not match certificate common name*",
    .func = (LanFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_TLS_IDENTITY,
    .errmsg = "*device ID does not match certificate common name*",
    .func = (LanFixtureFunc)test_lan_service_outgoing_broadcast,
  },
};

static void
test_lan_service_compliance_test (gconstpointer user_data)
{
  LanTestCase *test_case = (LanTestCase *)user_data;
  const char *test_name = g_test_get_path ();

  if (g_test_subprocess ())
    {
      g_autofree LanTestFixture *fixture = NULL;
      JsonObject *body = NULL;

      fixture = g_new0 (LanTestFixture, 1);
      lan_service_fixture_set_up (fixture, test_case);

      body = valent_packet_get_body (fixture->peer_identity);
      if (g_strcmp0 (test_name, TEST_INCOMING_IDENTITY_OVERSIZE) == 0 ||
          g_strcmp0 (test_name, TEST_OUTGOING_IDENTITY_OVERSIZE) == 0)
        {
          g_autofree char *oversize = NULL;

          oversize = g_strnfill (IDENTITY_BUFFER_MAX + 1, '0');
          json_object_set_string_member (body, "oversize", oversize);
        }
      else if (g_strcmp0 (test_name, TEST_INCOMING_INVALID_ID) == 0 ||
               g_strcmp0 (test_name, TEST_OUTGOING_INVALID_ID) == 0)
        {
          json_object_set_string_member (body, "deviceId", "!@#$%^&*()");
        }
      else if (g_strcmp0 (test_name, TEST_INCOMING_INVALID_NAME) == 0 ||
               g_strcmp0 (test_name, TEST_OUTGOING_INVALID_NAME) == 0)
        {
          json_object_set_string_member (body, "deviceName", "!@#$%^&*()");
        }
      else if (g_strcmp0 (test_name, TEST_INCOMING_TLS_CERTIFICATE) == 0 ||
               g_strcmp0 (test_name, TEST_OUTGOING_TLS_CERTIFICATE) == 0)
        {
          g_autofree char *peer_dir = NULL;
          g_autofree char *peer_pem = NULL;
          const char *peer_id = NULL;

          /* Install a certificate for the peer that won't match the one
           * presented during the TLS handshake.
           *
           * TODO: test the case where the certificate common name matches the
           *       device ID, but the certificate is different.
           */
          peer_id = valent_certificate_get_common_name (fixture->peer_certificate);
          peer_dir = g_build_filename (g_get_user_config_dir(), "valent",
                                       "device", peer_id,
                                       NULL);
          g_mkdir_with_parents (peer_dir, 0700);

          g_clear_object (&fixture->peer_certificate);
          fixture->peer_certificate = valent_certificate_new_sync (peer_dir, NULL);
        }
      else if (g_strcmp0 (test_name, TEST_INCOMING_TLS_IDENTITY) == 0 ||
               g_strcmp0 (test_name, TEST_OUTGOING_TLS_IDENTITY) == 0)
        {
          /* Connect using a certificate with a common name that won't match
           * the `deviceId` field in the identity packet.
           *
           * TODO: test the case where the certificate common name matches the
           *       device ID, but the certificate is different.
           */
          g_clear_object (&fixture->peer_certificate);
          fixture->peer_certificate = valent_certificate_new_sync (NULL, NULL);
        }

      test_case->func (fixture, test_case);
      lan_service_fixture_tear_down (fixture, test_case);
    }
  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_stderr (test_case->errmsg);
  g_test_trap_assert_failed ();
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_LAN_CHANNEL);
  g_type_ensure (VALENT_TYPE_LAN_CHANNEL_SERVICE);

  g_test_add ("/plugins/lan/incoming-broadcast",
              LanTestFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_incoming_broadcast,
              lan_service_fixture_tear_down);

  g_test_add ("/plugins/lan/outgoing-broadcast",
              LanTestFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_outgoing_broadcast,
              lan_service_fixture_tear_down);

  for (size_t i = 0; i < G_N_ELEMENTS (compliance_tests); i++)
    {
      g_test_add_data_func (compliance_tests[i].name,
                            &compliance_tests[i],
                            test_lan_service_compliance_test);
    }

  // TODO: runs last, since it consistently fails under clang
  g_test_add ("/plugins/lan/channel",
              LanTestFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_channel,
              lan_service_fixture_tear_down);

  return g_test_run ();
}
