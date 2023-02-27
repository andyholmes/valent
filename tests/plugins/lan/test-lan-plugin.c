// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

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
#define ENDPOINT_ADDR          "127.0.0.1:1717"
#define ENDPOINT_HOST          "127.0.0.1"
#define ENDPOINT_PORT          (1717)
#define SERVICE_ADDR           "127.0.0.1:1718"
#define SERVICE_HOST           "127.0.0.1"
#define SERVICE_PORT           (1718)

#define IDENTITY_BUFFER_MAX    (8192)

#define TEST_IDENTITY_OVERSIZE "identity-oversize"
#define TEST_IDENTITY_TIMEOUT  "identity-timeout"
#define TEST_TLS_AUTH_TIMEOUT  "tls-auth-timeout"
#define TEST_TLS_AUTH_SPOOFER  "tls-auth-spoofer"


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
  JsonNode *identity;
  GError *error = NULL;

  fixture->loop = g_main_loop_new (NULL, FALSE);

  plugin_info = peas_engine_get_plugin_info (valent_get_plugin_engine (), "lan");
  fixture->service = g_object_new (VALENT_TYPE_LAN_CHANNEL_SERVICE,
                                   "id",                "test-device",
                                   "broadcast-address", "127.0.0.255",
                                   "port",              SERVICE_PORT,
                                   "plugin-info",       plugin_info,
                                   NULL);

  fixture->packets = valent_test_load_json ("plugin-lan.json");
  fixture->socket = create_socket ();

  /* Generate a certificate for the phony client */
  path = g_dir_make_tmp ("XXXXXX.valent", NULL);
  fixture->certificate = valent_certificate_new_sync (path, &error);
  g_assert_no_error (error);

  /* Set the endpoint deviceId to the certificate's common name */
  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");
  json_object_set_string_member (valent_packet_get_body (identity),
                                 "deviceId",
                                 valent_certificate_get_common_name (fixture->certificate));
}

static void
lan_service_fixture_tear_down (LanBackendFixture *fixture,
                               gconstpointer      user_data)
{
  g_clear_pointer (&fixture->loop, g_main_loop_unref);
  g_clear_pointer (&fixture->packets, json_node_unref);

  v_await_finalize_object (fixture->service);
  v_await_finalize_object (fixture->channel);
  v_await_finalize_object (fixture->endpoint);

  v_assert_finalize_object (fixture->certificate);
  v_assert_finalize_object (fixture->socket);
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
  const char *device_id = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;
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
  g_assert_true (device_id != NULL && *device_id != '\0');

  tls_stream = valent_lan_encrypt_client_connection (connection,
                                                     fixture->certificate,
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
                                    "host",          SERVICE_HOST,
                                    "port",          SERVICE_PORT,
                                    "identity",      identity,
                                    "peer-identity", peer_identity,
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

static gboolean
on_timeout_cb (gpointer data)
{
  gboolean *done = data;

  if (*done == FALSE)
    *done = TRUE;

  return G_SOURCE_REMOVE;
}

static void
await_timeout (unsigned int interval)
{
  gboolean done = FALSE;

  g_timeout_add (interval, on_timeout_cb, &done);

  while (!done)
    g_main_context_iteration (NULL, FALSE);
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
g_async_initable_init_async_cb (GAsyncInitable    *initable,
                                GAsyncResult      *result,
                                LanBackendFixture *fixture)
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
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

static void
test_lan_service_incoming_broadcast_oversize (void)
{
  if (g_test_subprocess ())
    {
      LanBackendFixture *fixture;
      JsonNode *identity;
      g_autofree char *oversize = NULL;

      /* Perform fixture setup */
      fixture = g_new0 (LanBackendFixture, 1);
      lan_service_fixture_set_up (fixture, NULL);

      /* Inject data into the identity packet, to force it to be rejected */
      identity = json_object_get_member (json_node_get_object (fixture->packets),
                                         "identity");
      oversize = g_strnfill (IDENTITY_BUFFER_MAX + 1, '0');
      json_object_set_string_member (valent_packet_get_body (identity),
                                     "oversize",
                                     oversize);

      /* Run the test to be failed */
      test_lan_service_incoming_broadcast (fixture, TEST_IDENTITY_OVERSIZE);

      /* Perform fixture teardown */
      lan_service_fixture_tear_down (fixture, NULL);
      g_clear_pointer (&fixture, g_free);

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
}

static void
test_lan_service_outgoing_broadcast (LanBackendFixture *fixture,
                                     gconstpointer      user_data)
{
  g_autoptr (GInputStream) unix_stream = NULL;
  g_autoptr (GDataInputStream) data_stream = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  g_autoptr (GSocketClient) client = NULL;
  g_autoptr (GSocketConnection) connection = NULL;
  JsonNode *identity;
  GOutputStream *output_stream;
  g_autoptr (GIOStream) tls_stream = NULL;
  GError *error = NULL;

  g_async_initable_init_async (G_ASYNC_INITABLE (fixture->service),
                               G_PRIORITY_DEFAULT,
                               NULL,
                               (GAsyncReadyCallback)g_async_initable_init_async_cb,
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
                                       &peer_identity);

  while (peer_identity == NULL)
    g_main_context_iteration (NULL, FALSE);

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

  while (connection == NULL)
    g_main_context_iteration (NULL, FALSE);

  /* We opened a TCP connection in response to the incoming UDP broadcast so the
   * test service now expects us to write our identity packet.
   */
  if (g_test_subprocess ())
    {
      /* In this test case we are neglecting to send our identity packet, so
       * we expect the test service to close the connection after 1000ms */
      if (g_strcmp0 (user_data, TEST_IDENTITY_TIMEOUT) == 0)
        await_timeout (1100);
    }

  identity = json_object_get_member (json_node_get_object (fixture->packets),
                                     "identity");

  output_stream = g_io_stream_get_output_stream (G_IO_STREAM (connection));
  valent_packet_to_stream (output_stream, identity, NULL, &error);
  g_assert_no_error (error);

  /* The test service is unverified, so it expects to be accepted on a
   * trust-on-first-use basis.
   */
  if (g_test_subprocess ())
    {
      /* In this test case we are neglecting to negotiate a TLS connection, so
       * we expect the test service to close the connection after 1000ms */
      if (g_strcmp0 (user_data, TEST_TLS_AUTH_TIMEOUT) == 0)
        await_timeout (1100);
    }

  tls_stream = valent_lan_encrypt_server_connection (connection,
                                                     fixture->certificate,
                                                     NULL,
                                                     &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_TLS_CONNECTION (tls_stream));

  /* We're pretending to be a remote service, so we create an endpoint channel
   * so that we can pop packets of it from the test service.
   */
  fixture->endpoint = g_object_new (VALENT_TYPE_LAN_CHANNEL,
                                    "base-stream",   tls_stream,
                                    "host",          SERVICE_HOST,
                                    "port",          SERVICE_PORT,
                                    "identity",      identity,
                                    "peer-identity", peer_identity,
                                    NULL);

  /* When the test service accepts the incoming connection, it should negotiate
   * the TLS connection and create a channel.
   */
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  g_main_loop_run (fixture->loop);

  /* In this test case we are trying to connect with the same device ID and a
   * different certificate, so we expect the service to reject the connection.
   */
  if (g_strcmp0 (user_data, TEST_TLS_AUTH_SPOOFER) == 0)
    {
      g_autoptr (GSocketClient) bad_client = NULL;
      g_autoptr (GSocketConnection) bad_connection = NULL;
      g_autoptr (GIOStream) bad_stream = NULL;
      g_autoptr (GTlsCertificate) bad_certificate = NULL;

      bad_client = g_object_new (G_TYPE_SOCKET_CLIENT,
                                 "enable-proxy", FALSE,
                                 NULL);
      g_socket_client_connect_to_host_async (bad_client,
                                             SERVICE_ADDR,
                                             SERVICE_PORT,
                                             NULL,
                                             (GAsyncReadyCallback)g_socket_client_connect_to_host_cb,
                                             &bad_connection);

      while (bad_connection == NULL)
        g_main_context_iteration (NULL, FALSE);

      output_stream = g_io_stream_get_output_stream (G_IO_STREAM (bad_connection));
      valent_packet_to_stream (output_stream, identity, NULL, &error);
      g_assert_no_error (error);

      /* HACK: we're just sending the service's certificate back to itself,
       *       so the common name won't match the `deviceId` in the identity
       * TODO: test the case where the certificate common name _does_ match the
       *       identity, but the certificate itself is different
       */
      g_object_get (fixture->service, "certificate", &bad_certificate, NULL);
      bad_stream = valent_lan_encrypt_server_connection (bad_connection,
                                                         bad_certificate,
                                                         NULL,
                                                         &error);
      g_assert_no_error (error);
      g_assert_true (G_IS_TLS_CONNECTION (bad_stream));
    }

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

static void
test_lan_service_outgoing_broadcast_oversize (void)
{
  if (g_test_subprocess ())
    {
      LanBackendFixture *fixture;
      JsonNode *identity;
      g_autofree char *oversize = NULL;

      /* Perform fixture setup */
      fixture = g_new0 (LanBackendFixture, 1);
      lan_service_fixture_set_up (fixture, NULL);

      /* Inject data into the identity packet, to force it to be rejected */
      identity = json_object_get_member (json_node_get_object (fixture->packets),
                                         "identity");
      oversize = g_strnfill (IDENTITY_BUFFER_MAX + 1, '0');
      json_object_set_string_member (valent_packet_get_body (identity),
                                     "oversize",
                                     oversize);

      /* Run the test to be failed */
      test_lan_service_outgoing_broadcast (fixture, TEST_IDENTITY_OVERSIZE);

      /* Perform fixture teardown */
      lan_service_fixture_tear_down (fixture, NULL);
      g_clear_pointer (&fixture, g_free);

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr ("*Packet too large*");
  g_test_trap_assert_failed ();
}

static void
test_lan_service_outgoing_broadcast_timeout (void)
{
  if (g_test_subprocess ())
    {
      LanBackendFixture *fixture;

      /* Perform fixture setup */
      fixture = g_new0 (LanBackendFixture, 1);
      lan_service_fixture_set_up (fixture, NULL);

      /* Run the test to be failed */
      test_lan_service_outgoing_broadcast (fixture, TEST_IDENTITY_TIMEOUT);

      /* Perform fixture teardown */
      lan_service_fixture_tear_down (fixture, NULL);
      g_clear_pointer (&fixture, g_free);

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
}

static void
test_lan_service_outgoing_broadcast_tls_timeout (void)
{
  if (g_test_subprocess ())
    {
      LanBackendFixture *fixture;

      /* Perform fixture setup */
      fixture = g_new0 (LanBackendFixture, 1);
      lan_service_fixture_set_up (fixture, NULL);

      /* Run the test to be failed */
      test_lan_service_outgoing_broadcast (fixture, TEST_TLS_AUTH_TIMEOUT);

      /* Perform fixture teardown */
      lan_service_fixture_tear_down (fixture, NULL);
      g_clear_pointer (&fixture, g_free);

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_failed ();
}

static void
test_lan_service_outgoing_broadcast_tls_spoofer (void)
{
  if (g_test_subprocess ())
    {
      LanBackendFixture *fixture;

      /* Perform fixture setup */
      fixture = g_new0 (LanBackendFixture, 1);
      lan_service_fixture_set_up (fixture, NULL);

      /* Run the test to be failed */
      test_lan_service_outgoing_broadcast (fixture, TEST_TLS_AUTH_SPOOFER);

      /* Perform fixture teardown */
      lan_service_fixture_tear_down (fixture, NULL);
      g_clear_pointer (&fixture, g_free);

      return;
    }
  g_test_trap_subprocess (NULL, 0, 0);
  g_test_trap_assert_stderr ("*device ID does not match certificate common name*");
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
  const char *channel_verification;
  const char *endpoint_verification;
  char *host;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  guint16 port;
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

  /* Properties */
  g_object_get (fixture->channel,
                "certificate", &certificate,
                "host",        &host,
                "port",        &port,
                NULL);
  /* FIXME: the call to `g_object_get()` for "peer-certificate" must come after
   *        and must be a separate call. If not, this will segfault, but only on
   *        Clang with ASan not enabled. */
  // TODO: valgrind
  g_object_get (fixture->channel,
                "peer-certificate", &peer_certificate,
                NULL);

  g_assert_true (G_IS_TLS_CERTIFICATE (certificate));
  g_assert_true (G_IS_TLS_CERTIFICATE (peer_certificate));
  g_clear_object (&certificate);
  g_clear_object (&peer_certificate);

  certificate = valent_lan_channel_ref_certificate (VALENT_LAN_CHANNEL (fixture->endpoint));
  peer_certificate = valent_lan_channel_ref_peer_certificate (VALENT_LAN_CHANNEL (fixture->channel));
  g_assert_true (g_tls_certificate_is_same (certificate, peer_certificate));
  g_clear_object (&certificate);
  g_clear_object (&peer_certificate);

  certificate = valent_lan_channel_ref_certificate (VALENT_LAN_CHANNEL (fixture->channel));
  peer_certificate = valent_lan_channel_ref_peer_certificate (VALENT_LAN_CHANNEL (fixture->endpoint));
  g_assert_true (g_tls_certificate_is_same (certificate, peer_certificate));
  g_clear_object (&certificate);
  g_clear_object (&peer_certificate);

  g_assert_cmpstr (host, ==, ENDPOINT_HOST);
  g_assert_cmpuint (port, ==, ENDPOINT_PORT);
  g_free (host);

  channel_verification = valent_channel_get_verification_key (fixture->channel);
  endpoint_verification = valent_channel_get_verification_key (fixture->endpoint);
  g_assert_nonnull (channel_verification);
  g_assert_nonnull (endpoint_verification);
  g_assert_cmpstr (channel_verification, ==, endpoint_verification);

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

  g_test_add_func ("/plugins/lan/incoming-broadcast-oversize",
                   test_lan_service_incoming_broadcast_oversize);

  g_test_add ("/plugins/lan/outgoing-broadcast",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_outgoing_broadcast,
              lan_service_fixture_tear_down);

  g_test_add_func ("/plugins/lan/outgoing-broadcast-oversize",
                   test_lan_service_outgoing_broadcast_oversize);

  g_test_add_func ("/plugins/lan/outgoing-broadcast-timeout",
                   test_lan_service_outgoing_broadcast_timeout);

  g_test_add_func ("/plugins/lan/outgoing-broadcast-tls-auth",
                   test_lan_service_outgoing_broadcast_tls_timeout);

  g_test_add_func ("/plugins/lan/outgoing-broadcast-tls-cert",
                   test_lan_service_outgoing_broadcast_tls_spoofer);

  g_test_add ("/plugins/lan/channel",
              LanBackendFixture, NULL,
              lan_service_fixture_set_up,
              test_lan_service_channel,
              lan_service_fixture_tear_down);

  return g_test_run ();
}
