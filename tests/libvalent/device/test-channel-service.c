// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <valent.h>
#include <libvalent-test.h>

#include "valent-mock-channel.h"
#include "valent-mock-channel-service.h"


typedef struct
{
  JsonNode             *packets;

  ValentChannelService *service;
  ValentChannel        *channel;
  ValentChannel        *endpoint;

  gpointer              data;
} ChannelServiceFixture;

static void
channel_service_fixture_set_up (ChannelServiceFixture *fixture,
                                gconstpointer      user_data)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;

  fixture->packets = valent_test_load_json ("core.json");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  fixture->service = g_object_new (VALENT_TYPE_MOCK_CHANNEL_SERVICE,
                                   "iri",         "urn:valent:network:mock",
                                   // FIXME: root source
                                   "source",      NULL,
                                   "context",     context,
                                   "plugin-info", plugin_info,
                                   NULL);
}

static void
channel_service_fixture_tear_down (ChannelServiceFixture *fixture,
                                   gconstpointer      user_data)
{
  if (fixture->service != NULL)
    {
      valent_object_destroy (VALENT_OBJECT (fixture->service));
      v_await_finalize_object (fixture->service);
    }

  if (fixture->channel != NULL)
    {
      valent_object_destroy (VALENT_OBJECT (fixture->channel));
      v_await_finalize_object (fixture->channel);
    }

  if (fixture->endpoint != NULL)
    {
      valent_object_destroy (VALENT_OBJECT (fixture->endpoint));
      v_await_finalize_object (fixture->endpoint);
    }

  g_clear_pointer (&fixture->packets, json_node_unref);
}

/*
 * ValentChannelService Callbacks
 */
static void
on_channel (ValentChannelService  *service,
            ValentChannel         *channel,
            ChannelServiceFixture *fixture)
{
  fixture->channel = g_object_ref (channel);
  fixture->endpoint = g_object_ref (g_object_get_data (G_OBJECT (service),
                                                       "valent-peer-channel"));
}

static void
test_channel_service_service (ChannelServiceFixture *fixture,
                              gconstpointer          user_data)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autofree char *id = NULL;
  g_autoptr (JsonNode) identity = NULL;

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->service,
                "certificate", &certificate,
                "id",          &id,
                "identity",    &identity,
                NULL);

  g_assert_true (G_IS_TLS_CERTIFICATE (certificate));
  g_assert_nonnull (id);
  g_assert_true (VALENT_IS_PACKET (identity));
  g_clear_pointer (&id, g_free);

  VALENT_TEST_CHECK ("The service ID matches the certificate common name");
  id = valent_channel_service_dup_id (fixture->service);
  g_assert_cmpstr (id, ==, valent_certificate_get_common_name (certificate));

  VALENT_TEST_CHECK ("The service creates channels for successful connections");
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  valent_channel_service_identify (fixture->service, NULL);

  valent_test_await_pointer (&fixture->channel);
  g_assert_true (VALENT_IS_CHANNEL (fixture->channel));
  g_assert_true (VALENT_IS_CHANNEL (fixture->endpoint));

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
}

/*
 * Channel
 */
static void
valent_channel_read_packet_cb (ValentChannel *channel,
                               GAsyncResult  *result,
                               gpointer       user_data)
{
  g_autoptr (JsonNode) packet = NULL;
  GError *error = NULL;

  packet = valent_channel_read_packet_finish (channel, result, &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (packet));

  v_assert_packet_type (packet, "kdeconnect.mock.echo");
  valent_test_quit_loop ();
}

static void
valent_channel_write_packet_cb (ValentChannel *channel,
                                GAsyncResult  *result,
                                gpointer       user_data)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_channel_write_packet_finish (channel, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  valent_test_quit_loop ();
}

static void
valent_test_upload_cb (ValentChannel *channel,
                       GAsyncResult  *result,
                       gboolean      *done)
{
  GError *error = NULL;

  *done = valent_test_upload_finish (channel, result, &error);
  g_assert_no_error (error);
}

static void
valent_test_download_cb (ValentChannel *channel,
                         GAsyncResult  *result,
                         gboolean      *done)
{
  GError *error = NULL;

  *done = valent_test_download_finish (channel, result, &error);
  g_assert_no_error (error);
}

static void
valent_channel_read_download_cb (ValentChannel *endpoint,
                                 GAsyncResult  *result,
                                 gboolean      *done)
{
  g_autoptr (JsonNode) packet = NULL;
  GError *error = NULL;

  packet = valent_channel_read_packet_finish (endpoint, result, &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (packet));
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_download (endpoint,
                        packet,
                        NULL,
                        (GAsyncReadyCallback)valent_test_download_cb,
                        done);
}

static void
valent_channel_close_cb (ValentChannel *channel,
                         GAsyncResult  *result,
                         gpointer       user_data)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_channel_close_finish (channel, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  valent_test_quit_loop ();
}

static void
test_channel_service_channel (ChannelServiceFixture *fixture,
                              gconstpointer          user_data)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GIOStream) base_stream = NULL;
  g_autoptr (JsonNode) identity = NULL;
  g_autoptr (JsonNode) peer_identity = NULL;
  g_autoptr (GTlsCertificate) certificate = NULL;
  g_autoptr (GTlsCertificate) peer_certificate = NULL;
  g_autoptr (GTlsCertificate) endpoint_certificate = NULL;
  g_autoptr (GTlsCertificate) endpoint_peer_certificate = NULL;
  g_autoptr (GFile) file = NULL;
  gboolean download_done = FALSE;
  gboolean upload_done = FALSE;

  VALENT_TEST_CHECK ("The service creates channels for successful connections");
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  valent_channel_service_identify (fixture->service, NULL);
  valent_test_await_pointer (&fixture->channel);
  g_assert_true (VALENT_IS_CHANNEL (fixture->channel));

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->channel,
                "base-stream",      &base_stream,
                "certificate",      &certificate,
                "identity",         &identity,
                "peer-certificate", &peer_certificate,
                "peer-identity",    &peer_identity,
                NULL);

  g_assert_true (G_IS_IO_STREAM (base_stream));
  g_assert_true (G_IS_TLS_CERTIFICATE (certificate));
  g_assert_true (G_IS_TLS_CERTIFICATE (peer_certificate));
  g_assert_true (VALENT_IS_PACKET (identity));
  g_assert_true (VALENT_IS_PACKET (peer_identity));

  endpoint_certificate = valent_channel_ref_certificate (fixture->endpoint);
  endpoint_peer_certificate = valent_channel_ref_peer_certificate (fixture->endpoint);
  g_assert_true (g_tls_certificate_is_same (certificate, endpoint_peer_certificate));
  g_assert_true (g_tls_certificate_is_same (peer_certificate, endpoint_certificate));
  g_assert_true (json_node_equal (valent_channel_get_identity (fixture->channel), identity));
  g_assert_true (json_node_equal (valent_channel_get_peer_identity (fixture->channel), peer_identity));

  VALENT_TEST_CHECK ("Channel can send and receive packets");
  packet = valent_packet_new ("kdeconnect.mock.echo");
  valent_channel_write_packet (fixture->channel,
                               packet,
                               NULL, // cancellable
                               (GAsyncReadyCallback)valent_channel_write_packet_cb,
                               NULL);
  valent_test_run_loop ();

  valent_channel_read_packet (fixture->endpoint,
                              NULL, // cancellable
                              (GAsyncReadyCallback)valent_channel_read_packet_cb,
                              NULL);
  valent_test_run_loop ();
  g_clear_pointer (&packet, json_node_unref);

  VALENT_TEST_CHECK ("Channel can transfer payloads");
  packet = valent_packet_new ("kdeconnect.mock.transfer");
  json_object_set_string_member (valent_packet_get_body (packet),
                                 "filename",
                                 "image.png");
  file = g_file_new_for_uri ("resource:///tests/image.png");

  valent_test_upload (fixture->channel,
                      packet,
                      file,
                      NULL, // cancellable
                      (GAsyncReadyCallback)valent_test_upload_cb,
                      &upload_done);
  valent_channel_read_packet (fixture->endpoint,
                              NULL, // cancellable
                              (GAsyncReadyCallback)valent_channel_read_download_cb,
                              &download_done);
  valent_test_await_boolean (&upload_done);
  valent_test_await_boolean (&download_done);

  VALENT_TEST_CHECK ("Channel can be closed");
  valent_channel_close_async (fixture->channel,
                              NULL, // cancellable
                              (GAsyncReadyCallback)valent_channel_close_cb,
                              NULL);
  valent_test_run_loop ();
  valent_channel_close (fixture->endpoint, NULL, NULL);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_MOCK_CHANNEL);
  g_type_ensure (VALENT_TYPE_MOCK_CHANNEL_SERVICE);

  g_test_add ("/libvalent/device/channel-service/service",
              ChannelServiceFixture, NULL,
              channel_service_fixture_set_up,
              test_channel_service_service,
              channel_service_fixture_tear_down);

  g_test_add ("/libvalent/device/channel-service/channel",
              ChannelServiceFixture, NULL,
              channel_service_fixture_set_up,
              test_channel_service_channel,
              channel_service_fixture_tear_down);

  return g_test_run ();
}
