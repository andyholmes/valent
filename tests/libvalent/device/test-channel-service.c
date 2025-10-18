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

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  fixture->packets = valent_test_load_json ("core.json");
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
  g_clear_pointer (&fixture->packets, json_node_unref);
  v_await_finalize_object (fixture->service);

  if (fixture->channel != NULL)
    v_await_finalize_object (fixture->channel);

  if (fixture->endpoint != NULL)
    v_await_finalize_object (fixture->endpoint);
}

/*
 * ValentChannelService Callbacks
 */
static void
on_channel (ValentChannelService  *service,
            ValentChannel         *channel,
            ChannelServiceFixture *fixture)
{
  fixture->channel = channel;
  g_object_add_weak_pointer (G_OBJECT (fixture->channel),
                             (gpointer)&fixture->channel);

  fixture->endpoint = g_object_get_data (G_OBJECT (service),
                                         "valent-test-endpoint");
  g_object_add_weak_pointer (G_OBJECT (fixture->endpoint),
                             (gpointer)&fixture->endpoint);
}


/*
 * ValentChannel Callbacks
 */
static void
close_cb (ValentChannel         *channel,
          GAsyncResult          *result,
          ChannelServiceFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_channel_close_finish (channel, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  valent_test_quit_loop ();
}

static void
read_packet_cb (ValentChannel         *channel,
                GAsyncResult          *result,
                ChannelServiceFixture *fixture)
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
write_packet_cb (ValentChannel         *channel,
                 GAsyncResult          *result,
                 ChannelServiceFixture *fixture)
{
  gboolean ret;
  GError *error = NULL;

  ret = valent_channel_write_packet_finish (channel, result, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  valent_test_quit_loop ();
}

/*
 * Payload Callbacks
 */
static void
g_output_stream_splice_cb (GOutputStream *target,
                           GAsyncResult  *result,
                           gssize        *transferred)
{
  gssize ret;
  GError *error = NULL;

  ret = g_output_stream_splice_finish (target, result, &error);
  g_assert_no_error (error);

  if (transferred != NULL)
    *transferred = ret;
}

static void
valent_channel_download_cb (ValentChannel  *channel,
                            GAsyncResult   *result,
                            GIOStream     **stream)
{
  GError *error = NULL;

  g_assert_nonnull (stream);

  *stream = valent_channel_download_finish (channel, result, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_IO_STREAM (*stream));
}

static void
read_download_cb (ValentChannel         *endpoint,
                  GAsyncResult          *result,
                  ChannelServiceFixture *fixture)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GOutputStream) target = NULL;
  goffset payload_size;
  gssize transferred = -2;
  GError *error = NULL;

  /* We expect the packet to be properly populated with payload information */
  packet = valent_channel_read_packet_finish (endpoint, result, &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (packet));
  g_assert_true (valent_packet_has_payload (packet));

  /* We expect to be able to create a transfer stream from the packet */
  valent_channel_download_async (endpoint,
                                 packet,
                                 NULL,
                                 (GAsyncReadyCallback)valent_channel_download_cb,
                                 &stream);
  valent_test_await_pointer (&stream);

  /* We expect to be able to transfer the full payload */
  target = g_memory_output_stream_new_resizable ();
  g_output_stream_splice_async (target,
                                g_io_stream_get_input_stream (stream),
                                (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                 G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                G_PRIORITY_DEFAULT,
                                NULL,
                                (GAsyncReadyCallback)g_output_stream_splice_cb,
                                &transferred);

  while (transferred == -2)
    g_main_context_iteration (NULL, FALSE);

  payload_size = valent_packet_get_payload_size (packet);
  g_assert_cmpint (transferred, ==, payload_size);
}

static void
valent_channel_upload_cb (ValentChannel *channel,
                          GAsyncResult  *result,
                          GFile         *file)
{
  g_autoptr (GIOStream) stream = NULL;
  g_autoptr (GFileInputStream) file_source = NULL;
  GError *error = NULL;

  stream = valent_channel_upload_finish (channel, result, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_IO_STREAM (stream));

  file_source = g_file_read (file, NULL, &error);
  g_assert_no_error (error);

  g_output_stream_splice_async (g_io_stream_get_output_stream (stream),
                                G_INPUT_STREAM (file_source),
                                (G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                 G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET),
                                G_PRIORITY_DEFAULT,
                                NULL,
                                (GAsyncReadyCallback)g_output_stream_splice_cb,
                                NULL);
}

static void
read_upload_cb (ValentChannel         *endpoint,
                GAsyncResult          *result,
                ChannelServiceFixture *fixture)
{
  g_autoptr (JsonNode) packet = NULL;
  GError *error = NULL;

  /* We expect the packet to be properly populated with payload information */
  packet = valent_channel_read_packet_finish (endpoint, result, &error);
  g_assert_no_error (error);
  g_assert_true (VALENT_IS_PACKET (packet));
  g_assert_true (valent_packet_has_payload (packet));

  valent_test_download (endpoint, packet, &error);
  g_assert_no_error (error);

  valent_test_quit_loop ();
}


static void
test_channel_service_basic (void)
{
  PeasEngine *engine;
  PeasPluginInfo *plugin_info;
  g_autoptr (ValentContext) context = NULL;
  g_autoptr (ValentChannelService) service = NULL;
  g_autoptr (GTlsCertificate) certificate_out = NULL;
  g_autofree char *id_out = NULL;
  g_autoptr (JsonNode) identity_out = NULL;

  engine = valent_get_plugin_engine ();
  plugin_info = peas_engine_get_plugin_info (engine, "mock");
  context = valent_context_new (NULL, "plugin", "mock");

  VALENT_TEST_CHECK ("Adapter can be constructed");
  service = g_object_new (VALENT_TYPE_MOCK_CHANNEL_SERVICE,
                          "iri",         "urn:valent:network:mock",
                          // FIXME: root source
                          "source",      NULL,
                          "context",     context,
                          "plugin-info", plugin_info,
                          NULL);

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (service,
                "certificate", &certificate_out,
                "id",          &id_out,
                "identity",    &identity_out,
                NULL);

  g_assert_true (G_IS_TLS_CERTIFICATE (certificate_out));
  g_assert_nonnull (id_out);
  g_assert_true (VALENT_IS_PACKET (identity_out));

  g_clear_pointer (&id_out, g_free);
  id_out = valent_channel_service_dup_id (service);
  g_assert_nonnull (id_out);
}

static void
test_channel_service_identify (ChannelServiceFixture *fixture,
                               gconstpointer          user_data)
{
  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  valent_channel_service_identify (fixture->service, NULL);

  valent_test_await_pointer (&fixture->channel);
  g_assert_true (VALENT_IS_CHANNEL (fixture->channel));

  valent_test_await_pointer (&fixture->endpoint);
  g_assert_true (VALENT_IS_CHANNEL (fixture->endpoint));

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

static void
test_channel_service_channel (ChannelServiceFixture *fixture,
                              gconstpointer          user_data)
{
  g_autoptr (JsonNode) packet = NULL;
  g_autoptr (GIOStream) base_stream_out = NULL;
  g_autoptr (JsonNode) identity_out = NULL;
  g_autoptr (JsonNode) peer_identity_out = NULL;
  g_autoptr (GTlsCertificate) certificate_out = NULL;
  g_autoptr (GTlsCertificate) peer_certificate_out = NULL;
  g_autoptr (GFile) file = NULL;
  GError *error = NULL;

  g_signal_connect (fixture->service,
                    "channel",
                    G_CALLBACK (on_channel),
                    fixture);
  valent_channel_service_identify (fixture->service, NULL);

  valent_test_await_pointer (&fixture->channel);
  g_assert_true (VALENT_IS_CHANNEL (fixture->channel));

  valent_test_await_pointer (&fixture->endpoint);
  g_assert_true (VALENT_IS_CHANNEL (fixture->endpoint));

  VALENT_TEST_CHECK ("GObject properties function correctly");
  g_object_get (fixture->channel,
                "base-stream",      &base_stream_out,
                "certificate",      &certificate_out,
                "identity",         &identity_out,
                "peer-certificate", &peer_certificate_out,
                "peer-identity",    &peer_identity_out,
                NULL);

  g_assert_true (G_IS_IO_STREAM (base_stream_out));
  g_assert_true (G_IS_TLS_CERTIFICATE (certificate_out));
  g_assert_true (G_IS_TLS_CERTIFICATE (peer_certificate_out));
  g_assert_true (VALENT_IS_PACKET (identity_out));
  g_assert_true (VALENT_IS_PACKET (peer_identity_out));

  g_assert_true (json_node_equal (valent_channel_get_identity (fixture->channel), identity_out));
  g_assert_true (json_node_equal (valent_channel_get_peer_identity (fixture->channel), peer_identity_out));

  VALENT_TEST_CHECK ("Channel can send and receive packets");
  packet = valent_packet_new ("kdeconnect.mock.echo");
  valent_channel_write_packet (fixture->channel,
                               packet,
                               NULL,
                               (GAsyncReadyCallback)write_packet_cb,
                               fixture);
  valent_test_run_loop ();

  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)read_packet_cb,
                              fixture);
  valent_test_run_loop ();
  g_clear_pointer (&packet, json_node_unref);

  VALENT_TEST_CHECK ("Channel can download payloads from packets");
  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)read_download_cb,
                              fixture);

  packet = valent_packet_new ("kdeconnect.mock.transfer");
  json_object_set_string_member (valent_packet_get_body (packet),
                                 "filename",
                                 "image.png");
  file = g_file_new_for_uri ("resource:///tests/image.png");

  valent_test_upload (fixture->channel, packet, file, &error);
  g_assert_no_error (error);

  /* NOTE: The `payloadTransferInfo` has been set by the previous test */
  VALENT_TEST_CHECK ("Channel can upload payloads with packets");
  valent_channel_upload_async (fixture->channel,
                               packet,
                               NULL,
                               (GAsyncReadyCallback)valent_channel_upload_cb,
                               file);
  valent_channel_read_packet (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)read_upload_cb,
                              fixture);
  valent_test_run_loop ();

  VALENT_TEST_CHECK ("Channel can be closed");
  valent_channel_close_async (fixture->endpoint,
                              NULL,
                              (GAsyncReadyCallback)close_cb,
                              fixture);
  valent_test_run_loop ();
  valent_channel_close (fixture->channel, NULL, NULL);

  g_signal_handlers_disconnect_by_data (fixture->service, fixture);
  valent_object_destroy (VALENT_OBJECT (fixture->service));
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_type_ensure (VALENT_TYPE_MOCK_CHANNEL);
  g_type_ensure (VALENT_TYPE_MOCK_CHANNEL_SERVICE);

  g_test_add_func ("/libvalent/device/channel-service/basic",
                   test_channel_service_basic);

  g_test_add ("/libvalent/device/channel-service/identify",
              ChannelServiceFixture, NULL,
              channel_service_fixture_set_up,
              test_channel_service_identify,
              channel_service_fixture_tear_down);

  g_test_add ("/libvalent/device/channel-service/channel",
              ChannelServiceFixture, NULL,
              channel_service_fixture_set_up,
              test_channel_service_channel,
              channel_service_fixture_tear_down);

  return g_test_run ();
}
