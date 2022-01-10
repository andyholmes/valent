// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libvalent-core.h>


static const char *corrupt_packet =
  "{"
  "  \"id\": 0,"
  "  \"type\": \"kdeconnect.identity\","
  "  \"body\": {"
  "    \"field\": false"
  "  }";


typedef struct
{
  JsonNode   *node;
  JsonObject *packets;
  JsonNode   *invalid_node;
  JsonObject *invalid_packets;
  JsonNode   *large_node;
} PacketFixture;


static void
packet_fixture_set_up (PacketFixture *fixture,
                       gconstpointer  user_data)
{
  g_autoptr (JsonParser) parser = NULL;

  parser = json_parser_new ();
  json_parser_load_from_file (parser, TEST_DATA_DIR"/core.json", NULL);
  fixture->node = json_parser_steal_root (parser);
  fixture->packets = json_node_get_object (fixture->node);

  json_parser_load_from_file (parser, TEST_DATA_DIR"/core-packet.json", NULL);
  fixture->invalid_node = json_parser_steal_root (parser);
  fixture->invalid_packets = json_node_get_object (fixture->invalid_node);

  json_parser_load_from_file (parser, TEST_DATA_DIR"/core-large.json", NULL);
  fixture->large_node = json_parser_steal_root (parser);
}

static void
packet_fixture_tear_down (PacketFixture *fixture,
                          gconstpointer  user_data)
{
  g_clear_pointer (&fixture->node, json_node_unref);
  g_clear_pointer (&fixture->invalid_node, json_node_unref);
  g_clear_pointer (&fixture->large_node, json_node_unref);
}

static void
test_packet_builder (void)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  gint64 id;
  const char *type;
  JsonObject *body;

  builder = valent_packet_start ("kdeconnect.mock");
  packet = valent_packet_finish (builder);
  g_assert_true (valent_packet_is_valid (packet));

  id = valent_packet_get_id (packet);
  g_assert_cmpint (id, ==, 0);

  type = valent_packet_get_type (packet);
  g_assert_cmpstr (type, ==, "kdeconnect.mock");

  body = valent_packet_get_body (packet);
  g_assert_nonnull (body);

  json_object_set_string_member (body, "deviceId", "device-id");
  g_assert_cmpstr (valent_identity_get_device_id (packet), ==, "device-id");
}

static void
test_packet_payloads (void)
{
  JsonNode *packet;
  JsonObject *payload_info;
  gint64 payload_size;
  GError *error = NULL;

  /* Field methods */
  packet = valent_packet_new ("kdeconnect.mock.transfer");
  payload_info = json_object_new ();
  json_object_set_int_member (payload_info, "port", 1739);
  valent_packet_set_payload_info (packet, g_steal_pointer (&payload_info));
  valent_packet_set_payload_size (packet, 42);
  payload_info = valent_packet_get_payload_info (packet);
  payload_size = valent_packet_get_payload_size (packet);

  g_assert_true (valent_packet_has_payload (packet));
  g_assert_cmpuint (42, ==, payload_size);
  g_assert_true (json_object_has_member (payload_info, "port"));

  json_node_unref (packet);

  /* Full methods */
  packet = valent_packet_new ("kdeconnect.mock.transfer");
  payload_info = json_object_new ();
  json_object_set_int_member (payload_info, "port", 1739);
  valent_packet_set_payload_full (packet, payload_info, 42);
  payload_info = valent_packet_get_payload_full (packet, &payload_size, &error);

  g_assert_no_error (error);
  g_assert_true (valent_packet_has_payload (packet));
  g_assert_cmpuint (42, ==, payload_size);
  g_assert_true (json_object_has_member (payload_info, "port"));

  json_node_unref (packet);
}

static void
test_packet_serializing (PacketFixture *fixture,
                         gconstpointer  user_data)
{
  JsonObjectIter iter;
  JsonNode *packet_in, *packet_out;
  char *packet_str = NULL;
  GError *error = NULL;

  json_object_iter_init (&iter, fixture->packets);

  while (json_object_iter_next (&iter, NULL, &packet_in))
    {
      packet_str = valent_packet_serialize (packet_in);
      packet_out = valent_packet_deserialize (packet_str, &error);
      g_assert_no_error (error);
      g_assert_true (json_node_equal (packet_in, packet_out));

      g_free (packet_str);
      g_clear_pointer (&packet_out, json_node_unref);
    }
}

static void
test_packet_invalid (PacketFixture *fixture,
                     gconstpointer  user_data)
{
  JsonObjectIter iter;
  JsonNode *packet;

  json_object_iter_init (&iter, fixture->invalid_packets);

  while (json_object_iter_next (&iter, NULL, &packet))
    {
      g_autoptr (GError) error = NULL;

      valent_packet_validate (packet, &error);
      /* g_assert_error (error, 0, 0); */
      g_assert_nonnull (error);
    }
}

static void
test_packet_streaming (PacketFixture *fixture,
                       gconstpointer  user_data)
{
  JsonObjectIter iter;
  JsonNode *packet_in, *packet_out;
  GInputStream *in = NULL;
  GOutputStream *out = NULL;
  g_autofree char *packet_str = NULL;
  g_autoptr (GBytes) bytes = NULL;
  GError *error = NULL;

  /* Write packets */
  out = g_memory_output_stream_new_resizable ();
  json_object_iter_init (&iter, fixture->packets);

  while (json_object_iter_next (&iter, NULL, &packet_in))
    {
      valent_packet_to_stream (out, packet_in, NULL, &error);
      g_assert_no_error (error);
    }

  g_output_stream_close (out, NULL, &error);
  g_assert_no_error (error);

  /* Read packets */
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out));
  in = g_memory_input_stream_new_from_bytes (bytes);
  json_object_iter_init (&iter, fixture->packets);

  while (json_object_iter_next (&iter, NULL, &packet_in))
    {
      packet_out = valent_packet_from_stream (in, NULL, &error);
      g_assert_no_error (error);

      g_assert_true (json_node_equal (packet_in, packet_out));
      g_clear_pointer (&packet_out, json_node_unref);
    }

  g_clear_object (&out);
  g_clear_object (&in);
  g_clear_pointer (&bytes, g_bytes_unref);

  /* Large input */
  packet_str = json_to_string (fixture->large_node, FALSE);
  in = g_memory_input_stream_new_from_data (packet_str, -1, NULL);
  packet_out = valent_packet_from_stream (in, NULL, &error);
  g_assert_no_error (error);
  g_clear_object (&in);
  g_clear_pointer (&packet_out, json_node_unref);

  /* Invalid input */
  in = g_memory_input_stream_new_from_data (corrupt_packet,
                                            strlen (corrupt_packet),
                                            NULL);
  packet_out = valent_packet_from_stream (in, NULL, &error);
  g_assert_error (error, JSON_PARSER_ERROR, JSON_PARSER_ERROR_INVALID_BAREWORD);
  g_clear_object (&in);
  g_clear_pointer (&packet_out, json_node_unref);
  g_clear_error (&error);

  in = g_memory_input_stream_new_from_data ("", 0, NULL);
  g_input_stream_close (in, NULL, NULL);
  packet_out = valent_packet_from_stream (in, NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_clear_object (&in);
  g_clear_error (&error);

  in = g_memory_input_stream_new_from_data ("", 0, NULL);
  packet_out = valent_packet_from_stream (in, NULL, &error);
  g_assert_error (error, VALENT_PACKET_ERROR, VALENT_PACKET_ERROR_INVALID_DATA);
  g_clear_object (&in);
  g_clear_error (&error);

  in = g_memory_input_stream_new_from_data ("\n", 1, NULL);
  packet_out = valent_packet_from_stream (in, NULL, &error);
  g_assert_error (error, VALENT_PACKET_ERROR, VALENT_PACKET_ERROR_INVALID_DATA);
  g_clear_object (&in);
  g_clear_error (&error);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/core/packet/builder",
                   test_packet_builder);

  g_test_add_func ("/core/packet/payloads",
                   test_packet_payloads);

  g_test_add ("/core/packet/invalid",
              PacketFixture, NULL,
              packet_fixture_set_up,
              test_packet_invalid,
              packet_fixture_tear_down);

  g_test_add ("/core/packet/serializing",
              PacketFixture, NULL,
              packet_fixture_set_up,
              test_packet_serializing,
              packet_fixture_tear_down);

  g_test_add ("/core/packet/streaming",
              PacketFixture, NULL,
              packet_fixture_set_up,
              test_packet_streaming,
              packet_fixture_tear_down);

  return g_test_run ();
}
