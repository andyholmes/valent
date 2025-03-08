// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-lan-utils.h"
#include "valent-lan-channel.h"
#include "valent-lan-channel-service.h"

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

#define IDENTITY_BUFFER_MAX             (8192)
#if VALENT_HAVE_ASAN
  #define AUTHENTICATION_TIMEOUT_MAX    (5500)
#else
  #define AUTHENTICATION_TIMEOUT_MAX    (1100)
#endif

#define TEST_INCOMING_IDENTITY_OVERSIZE "/plugins/lan/incoming-identity-oversize"
#define TEST_INCOMING_IDENTITY_TIMEOUT  "/plugins/lan/incoming-identity-timeout"
#define TEST_INCOMING_INVALID_ID        "/plugins/lan/incoming-invalid-id"
#define TEST_INCOMING_INVALID_NAME      "/plugins/lan/incoming-invalid-name"
#define TEST_OUTGOING_IDENTITY_OVERSIZE "/plugins/lan/outgoing-identity-oversize"
#define TEST_OUTGOING_IDENTITY_TIMEOUT  "/plugins/lan/outgoing-identity-timeout"
#define TEST_OUTGOING_INVALID_ID        "/plugins/lan/outgoing-invalid-id"
#define TEST_OUTGOING_INVALID_NAME      "/plugins/lan/outgoing-invalid-name"

#define TEST_INCOMING_TLS_SPOOFER       "/plugins/lan/incoming-tls-spoofer"
#define TEST_INCOMING_TLS_TIMEOUT       "/plugins/lan/incoming-tls-timeout"
#define TEST_OUTGOING_TLS_SPOOFER       "/plugins/lan/outgoing-tls-spoofer"
#define TEST_OUTGOING_TLS_TIMEOUT       "/plugins/lan/outgoing-tls-timeout"


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
} LanBackendFixture;

typedef struct
{
  const char       *name;
  const char       *errmsg;
  GTestFixtureFunc  func;
} LanTestCase;

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
  g_autofree char *device_id = NULL;
  g_autoptr (ValentContext) context = NULL;
  PeasPluginInfo *plugin_info;
  g_autofree char *peer_path = NULL;
  const char *peer_id = NULL;
  GError *error = NULL;

  fixture->packets = valent_test_load_json ("plugin-lan.json");
  fixture->peer_identity = json_object_get_member (json_node_get_object (fixture->packets),
                                                   "peer-identity");
  fixture->socket = create_socket ();

  /* Generate peer certificate and update the identity packet.
   */
  peer_path = g_dir_make_tmp (NULL, &error);
  g_assert_no_error (error);
  fixture->peer_certificate = valent_certificate_new_sync (peer_path, &error);
  g_assert_no_error (error);

  peer_id = valent_certificate_get_common_name (fixture->peer_certificate);
  json_object_set_string_member (valent_packet_get_body (fixture->peer_identity),
                                 "deviceId", peer_id);

  /* Prepare the local test service */
  device_id = valent_device_generate_id ();
  context = valent_context_new (NULL, "network", device_id);
  plugin_info = peas_engine_get_plugin_info (valent_get_plugin_engine (), "lan");
  fixture->service = g_object_new (VALENT_TYPE_LAN_CHANNEL_SERVICE,
                                   "context",           context,
                                   "plugin-info",       plugin_info,
                                   "broadcast-address", "127.0.0.255",
                                   "port",              SERVICE_PORT,
                                   NULL);
}

static void
lan_service_fixture_tear_down (LanBackendFixture *fixture,
                               gconstpointer      user_data)
{
  g_clear_pointer (&fixture->packets, json_node_unref);

  v_await_finalize_object (fixture->service);

  if (fixture->channel != NULL)
    v_await_finalize_object (fixture->channel);
  if (fixture->endpoint != NULL)
    v_await_finalize_object (fixture->endpoint);

  v_await_finalize_object (fixture->peer_certificate);
  v_await_finalize_object (fixture->socket);
}

/*
 * Endpoint Service
 */
static void
g_socket_listener_accept_cb (GSocketListener   *listener,
                             GAsyncResult      *result,
                             LanBackendFixture *fixture)
{
  const char *test_name = g_test_get_path ();
  g_autoptr (GSocketConnection) connection = NULL;
  GTlsCertificate *peer_certificate = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  const char *device_id = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;
  int64_t protocol_version = VALENT_NETWORK_PROTOCOL_MAX;
  GError *error = NULL;

  connection = g_socket_listener_accept_finish (listener, result, NULL, &error);
  g_socket_listener_close (listener);
  g_assert_no_error (error);

  /* The incoming TCP connection is in response to the mock UDP packet we sent,
   * so we now expect the test service to write its identity packet.
   */
  peer_identity = valent_packet_from_stream (g_io_stream_get_input_stream (G_IO_STREAM (connection)),
                                             IDENTITY_BUFFER_MAX,
                                             NULL,
                                             &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (peer_identity));

  /* The test service is unverified, so we expect it to be accepted on a
   * trust-on-first-use basis.
   */
  valent_packet_get_string (peer_identity, "deviceId", &device_id);
  g_assert_true (valent_device_validate_id (device_id));

  /* In this test case we are trying to connect with the same device ID and a
   * different certificate, so we expect the service to reject the connection.
   */
  if (g_strcmp0 (test_name, TEST_INCOMING_TLS_SPOOFER) == 0)
    {
      g_autoptr (GTlsCertificate) bad_certificate = NULL;
      g_autofree char *tmpdir = NULL;

      /* TODO: test the case where the certificate common name _does_ match the
       *       identity, but the certificate itself is different
       */
      g_clear_object (&fixture->peer_certificate);
      tmpdir = g_dir_make_tmp (NULL, &error);
      g_assert_no_error (error);
      fixture->peer_certificate = valent_certificate_new_sync (tmpdir, &error);
      g_assert_no_error (error);
    }

  /* In this test case we are neglecting to negotiate a TLS connection,
   * so we expect the test service to close the connection after 1000ms
   */
  if (g_strcmp0 (test_name, TEST_INCOMING_TLS_TIMEOUT) == 0)
    {
      valent_test_await_timeout (AUTHENTICATION_TIMEOUT_MAX);
    }

  tls_stream = valent_lan_encrypt_client_connection (connection,
                                                     fixture->peer_certificate,
                                                     NULL,
                                                     &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CONNECTION (tls_stream));

  valent_packet_get_int (peer_identity, "protocolVersion", &protocol_version);
  if (protocol_version >= VALENT_NETWORK_PROTOCOL_V8)
    {
      g_autoptr (JsonNode) secure_identity = NULL;

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
on_incoming_broadcast (GDataInputStream  *stream,
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

static void
test_lan_service_incoming_broadcast (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  GError *error = NULL;
  g_autoptr (GSocketAddress) address = NULL;
  g_autofree char *identity_json = NULL;
  gboolean watch = FALSE;

  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               &watch);
  valent_test_await_boolean (&watch);

  /* Listen for an incoming TCP connection */
  await_incoming_connection (fixture);

  /* Identify the mock endpoint to the service */
  address = g_inet_socket_address_new_from_string (SERVICE_HOST, SERVICE_PORT);
  identity_json = valent_packet_serialize (fixture->peer_identity);

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
                    &fixture->channel);
  valent_test_await_pointer (&fixture->channel);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

static void
test_lan_service_outgoing_broadcast (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  const char *test_name = g_test_get_path ();
  g_autoptr (GInputStream) unix_stream = NULL;
  g_autoptr (GDataInputStream) data_stream = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  GTlsCertificate *peer_certificate = NULL;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  GOutputStream *output_stream;
  g_autoptr (GIOStream) tls_stream = NULL;
  int64_t protocol_version = VALENT_NETWORK_PROTOCOL_MAX;
  gboolean watch = FALSE;
  GError *error = NULL;

  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               &watch);
  valent_test_await_boolean (&watch);

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
                                       &peer_identity);
  valent_test_await_pointer (&peer_identity);

  /* The test service identity has been received and now we will respond by
   * opening a TCP connection to it. */
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
  if (g_strcmp0 (test_name, TEST_OUTGOING_IDENTITY_TIMEOUT) == 0)
    {
      valent_test_await_timeout (AUTHENTICATION_TIMEOUT_MAX);
    }

  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));
  valent_packet_to_stream (output_stream, fixture->peer_identity, NULL, &error);
  g_assert_no_error (error);

  /* In this test case we are trying to connect with the same device ID and a
   * different certificate, so we expect the service to reject the connection.
   */
  if (g_strcmp0 (test_name, TEST_OUTGOING_TLS_SPOOFER) == 0)
    {
      g_autoptr (GTlsCertificate) bad_certificate = NULL;
      g_autofree char *tmpdir = NULL;

      /* TODO: test the case where the certificate common name _does_ match the
       *       identity, but the certificate itself is different
       */
      g_clear_object (&fixture->peer_certificate);
      tmpdir = g_dir_make_tmp (NULL, &error);
      g_assert_no_error (error);
      fixture->peer_certificate = valent_certificate_new_sync (tmpdir, &error);
      g_assert_no_error (error);
    }

  /* In this test case we are neglecting to negotiate a TLS connection,
   * so we expect the test service to close the connection after 1000ms
   */
  if (g_strcmp0 (test_name, TEST_OUTGOING_TLS_TIMEOUT) == 0)
    {
      valent_test_await_timeout (AUTHENTICATION_TIMEOUT_MAX);
    }

  tls_stream = valent_lan_encrypt_server_connection (connection,
                                                     fixture->peer_certificate,
                                                     NULL,
                                                     &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CONNECTION (tls_stream));

  valent_packet_get_int (peer_identity, "protocolVersion", &protocol_version);
  if (protocol_version >= VALENT_NETWORK_PROTOCOL_V8)
    {
      g_autoptr (JsonNode) secure_identity = NULL;

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

  /* When the test service accepts the incoming connection, it should negotiate
   * the TLS connection and create a channel.
   */
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    &fixture->channel);
  valent_test_await_pointer (&fixture->channel);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

static void
test_lan_service_invalid_identity (LanBackendFixture *fixture,
                                   gconstpointer      user_data)
{
  LanTestCase *test_case = (LanTestCase *)user_data;
  const char *test_name = g_test_get_path ();

  if (g_test_subprocess ())
    {
      JsonObject *body = valent_packet_get_body (fixture->peer_identity);

      /* Inject data into the identity packet, to force it to be rejected
       */
      if (g_strcmp0 (test_name, TEST_INCOMING_IDENTITY_OVERSIZE) == 0 ||
          g_strcmp0 (test_name, TEST_OUTGOING_IDENTITY_OVERSIZE) == 0)
        {
          g_autofree char *oversize = NULL;

          oversize = g_strnfill (IDENTITY_BUFFER_MAX + 1, '0');
          json_object_set_string_member (body, "oversize", oversize);
        }
      /* Override the valid `deviceId`, to force it to be rejected
       */
      else if (g_strcmp0 (test_name, TEST_INCOMING_INVALID_ID) == 0 ||
          g_strcmp0 (test_name, TEST_OUTGOING_INVALID_ID) == 0)
        {
          json_object_set_string_member (body, "deviceId", "!@#$%^&*()");
        }
      /* Override the valid `deviceName`, to force it to be rejected
       */
      else if (g_strcmp0 (test_name, TEST_INCOMING_INVALID_NAME) == 0 ||
               g_strcmp0 (test_name, TEST_OUTGOING_INVALID_NAME) == 0)
        {
          json_object_set_string_member (body, "deviceName", "!@#$%^&*()");
        }

      test_case->func (fixture, test_case);
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr (test_case->errmsg);
  g_test_trap_assert_failed ();
}

static void
test_lan_service_tls_authentication (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  LanTestCase *test_case = (LanTestCase *)user_data;

  if (g_test_subprocess ())
    test_case->func (fixture, test_case);

  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr (test_case->errmsg);
  g_test_trap_assert_failed ();
}

static void
test_lan_service_channel (LanBackendFixture *fixture,
                          gconstpointer      user_data)
{
  GError *error = NULL;
  JsonNode *packet;
  g_autoptr (GSocketAddress) address = NULL;
  g_autofree char *identity_str = NULL;
  char *host;
  GTlsCertificate *certificate = NULL;
  GTlsCertificate *peer_certificate = NULL;
  uint16_t port;
  g_autoptr (GFile) file = NULL;
  gboolean watch = FALSE;

  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
                               &watch);
  valent_test_await_boolean (&watch);

  /* Listen for an incoming TCP connection */
  await_incoming_connection (fixture);

  /* Identify the mock endpoint to the service */
  address = g_inet_socket_address_new_from_string (SERVICE_HOST, SERVICE_PORT);
  packet = json_object_get_member (json_node_get_object (fixture->packets),
                                   "peer-identity");
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
                    &fixture->channel);
  valent_test_await_pointer (&fixture->channel);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->channel,
                "host", &host,
                "port", &port,
                NULL);

  g_assert_cmpstr (host, ==, ENDPOINT_HOST);
  g_assert_cmpuint (port, ==, ENDPOINT_PORT);
  g_free (host);

  certificate = valent_channel_get_certificate (fixture->endpoint);
  peer_certificate = valent_channel_get_peer_certificate (fixture->channel);
  g_assert_true (g_tls_certificate_is_same (certificate, peer_certificate));

  certificate = valent_channel_get_certificate (fixture->channel);
  peer_certificate = valent_channel_get_peer_certificate (fixture->endpoint);
  g_assert_true (g_tls_certificate_is_same (certificate, peer_certificate));

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

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

static LanTestCase identity_tests[] = {
  {
    .name = TEST_INCOMING_IDENTITY_OVERSIZE,
    .errmsg = "*unterminated string constant*",
    .func = (GTestFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_IDENTITY_OVERSIZE,
    .errmsg = "*Packet too large*",
    .func = (GTestFixtureFunc)test_lan_service_outgoing_broadcast,
  },
#if 0
  {
    .name = TEST_INCOMING_IDENTITY_TIMEOUT,
    .errmsg = "*timed out waiting for peer identity*",
    .func = (GTestFixtureFunc)test_lan_service_incoming_broadcast,
  },
#endif
  {
    .name = TEST_OUTGOING_IDENTITY_TIMEOUT,
    .errmsg = "*timed out waiting for peer identity*",
    .func = (GTestFixtureFunc)test_lan_service_outgoing_broadcast,
  },
  {
    .name = TEST_INCOMING_INVALID_ID,
    .errmsg = "*invalid device ID*",
    .func = (GTestFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_INVALID_ID,
    .errmsg = "*invalid device ID*",
    .func = (GTestFixtureFunc)test_lan_service_outgoing_broadcast,
  },
  {
    .name = TEST_INCOMING_INVALID_NAME,
    .errmsg = "*invalid device name*",
    .func = (GTestFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_INVALID_NAME,
    .errmsg = "*invalid device name*",
    .func = (GTestFixtureFunc)test_lan_service_outgoing_broadcast,
  },
};

static LanTestCase tls_tests[] = {
  {
    .name = TEST_INCOMING_TLS_SPOOFER,
    .errmsg = "*device ID does not match certificate common name*",
    .func = (GTestFixtureFunc)test_lan_service_incoming_broadcast,
  },
  {
    .name = TEST_OUTGOING_TLS_SPOOFER,
    .errmsg = "*device ID does not match certificate common name*",
    .func = (GTestFixtureFunc)test_lan_service_outgoing_broadcast,
  },
#if 0
  {
    .name = TEST_INCOMING_TLS_TIMEOUT,
    .errmsg = "*timed out waiting for authentication*",
    .func = (GTestFixtureFunc)test_lan_service_incoming_broadcast,
  },
#endif
  {
    .name = TEST_OUTGOING_TLS_TIMEOUT,
    .errmsg = "*timed out waiting for authentication*",
    .func = (GTestFixtureFunc)test_lan_service_outgoing_broadcast,
  },
};

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_LAN_CHANNEL);
  g_type_ensure (VALENT_TYPE_LAN_CHANNEL_SERVICE);

  g_test_add ("/plugins/lan/incoming-broadcast",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_incoming_broadcast,
              lan_service_fixture_tear_down);

  g_test_add ("/plugins/lan/outgoing-broadcast",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_outgoing_broadcast,
              lan_service_fixture_tear_down);

  for (size_t i = 0; i < G_N_ELEMENTS (identity_tests); i++)
    {
      g_test_add (identity_tests[i].name,
                  LanBackendFixture, &identity_tests[i],
                  lan_service_fixture_set_up,
                  test_lan_service_invalid_identity,
                  lan_service_fixture_tear_down);
    }

  for (size_t i = 0; i < G_N_ELEMENTS (tls_tests); i++)
    {
      g_test_add (tls_tests[i].name,
                  LanBackendFixture, &tls_tests[i],
                  lan_service_fixture_set_up,
                  test_lan_service_tls_authentication,
                  lan_service_fixture_tear_down);
    }

  g_test_add ("/plugins/lan/channel",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_channel,
              lan_service_fixture_tear_down);

  return g_test_run ();
}
