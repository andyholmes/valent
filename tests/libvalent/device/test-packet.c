// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>


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
  fixture->node = valent_test_load_json ("core.json");
  fixture->packets = json_node_get_object (fixture->node);

  fixture->invalid_node = valent_test_load_json ("core-packet.json");
  fixture->invalid_packets = json_node_get_object (fixture->invalid_node);

  fixture->large_node = valent_test_load_json ("core-large.json");
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
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  int64_t id;
  const char *type;
  JsonObject *body;

  valent_packet_init (&builder, "kdeconnect.mock");
  packet = valent_packet_end (&builder);
  g_assert_true (valent_packet_is_valid (packet));

  id = valent_packet_get_id (packet);
  g_assert_cmpint (id, ==, 0);

  type = valent_packet_get_type (packet);
  g_assert_cmpstr (type, ==, "kdeconnect.mock");

  body = valent_packet_get_body (packet);
  g_assert_nonnull (body);
}

static void
test_packet_get (void)
{
  g_autoptr (JsonBuilder) builder = NULL;
  g_autoptr (JsonNode) packet = NULL;
  gboolean boolean_value = FALSE;
  double double_value = 0.0;
  int64_t int_value = 0;
  const char *string_value;
  JsonArray *array_value;
  JsonObject *object_value;
  g_auto (GStrv) strv = NULL;

  valent_packet_init (&builder, "kdeconnect.mock");
  json_builder_set_member_name (builder, "boolean");
  json_builder_add_boolean_value (builder, TRUE);
  json_builder_set_member_name (builder, "double");
  json_builder_add_double_value (builder, 3.14);
  json_builder_set_member_name (builder, "int");
  json_builder_add_int_value (builder, 42);
  json_builder_set_member_name (builder, "string");
  json_builder_add_string_value (builder, "string");
  json_builder_set_member_name (builder, "array");
  json_builder_begin_array (builder);
  json_builder_add_string_value (builder, "kdeconnect.mock.echo");
  json_builder_add_string_value (builder, "kdeconnect.mock.transfer");
  json_builder_end_array (builder);
  json_builder_set_member_name (builder, "object");
  json_builder_begin_object (builder);
  json_builder_end_object (builder);
  packet = valent_packet_end (&builder);

  g_assert_true (valent_packet_is_valid (packet));

  g_assert_true (valent_packet_get_boolean (packet, "boolean", &boolean_value));
  g_assert_true (boolean_value);

  g_assert_true (valent_packet_get_double (packet, "double", &double_value));
  g_assert_cmpfloat (double_value, >=, 3.14);

  g_assert_true (valent_packet_get_int (packet, "int", &int_value));
  g_assert_cmpint (int_value, ==, 42);

  g_assert_true (valent_packet_get_string (packet, "string", &string_value));
  g_assert_cmpstr (string_value, ==, "string");

  g_assert_true (valent_packet_get_array (packet, "array", &array_value));
  g_assert_nonnull (array_value);

  g_assert_true (valent_packet_get_object (packet, "object", &object_value));
  g_assert_nonnull (object_value);

  strv = valent_packet_dup_strv (packet, "array");
  g_assert_nonnull (strv);
  g_assert_cmpuint (g_strv_length (strv), ==, 2);
}

static void
test_packet_payloads (void)
{
  JsonNode *packet;
  JsonObject *payload_info;
  int64_t payload_size;
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
valent_packet_to_stream_cb (GOutputStream *stream,
                            GAsyncResult  *result,
                            gboolean      *done)
{
  GError *error = NULL;

  *done = valent_packet_to_stream_finish (stream, result, &error);
  g_assert_no_error (error);
}

static void
valent_packet_from_stream_cb (GInputStream  *stream,
                              GAsyncResult  *result,
                              JsonNode     **packet)
{
  GError *error = NULL;

  *packet = valent_packet_from_stream_finish (stream, result, &error);
  g_assert_no_error (error);
}

static void
test_packet_streaming (PacketFixture *fixture,
                       gconstpointer  user_data)
{
  JsonObjectIter iter;
  JsonNode *packet_in = NULL;
  JsonNode *packet_out = NULL;
  GInputStream *in = NULL;
  GOutputStream *out = NULL;
  g_autoptr (GBytes) bytes = NULL;
  GError *error = NULL;

  VALENT_TEST_CHECK ("valent_packet_to_stream_async() can write packets");
  out = g_memory_output_stream_new_resizable ();
  json_object_iter_init (&iter, fixture->packets);

  while (json_object_iter_next (&iter, NULL, &packet_in))
    {
      gboolean success = FALSE;

      valent_packet_to_stream_async (out,
                                     packet_in,
                                     NULL,
                                     (GAsyncReadyCallback) valent_packet_to_stream_cb,
                                     &success);
      valent_test_await_boolean (&success);
    }

  g_output_stream_close (out, NULL, &error);
  g_assert_no_error (error);

  VALENT_TEST_CHECK ("valent_packet_from_stream_async() can read packets");
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out));
  in = g_memory_input_stream_new_from_bytes (bytes);
  json_object_iter_init (&iter, fixture->packets);

  while (json_object_iter_next (&iter, NULL, &packet_in))
    {
      valent_packet_from_stream_async (in,
                                       -1,
                                       NULL,
                                       (GAsyncReadyCallback) valent_packet_from_stream_cb,
                                       &packet_out);
      valent_test_await_pointer (&packet_out);
      g_assert_true (json_node_equal (packet_in, packet_out));
      g_clear_pointer (&packet_out, json_node_unref);
    }

  g_clear_object (&out);
  g_clear_object (&in);
  g_clear_pointer (&bytes, g_bytes_unref);

  VALENT_TEST_CHECK ("valent_packet_to_stream() can write large packets");
  out = g_memory_output_stream_new_resizable ();
  valent_packet_to_stream (out, fixture->large_node, NULL, &error);
  g_assert_no_error (error);
  g_output_stream_close (out, NULL, &error);
  g_assert_no_error (error);

  VALENT_TEST_CHECK ("valent_packet_from_stream() can read large packets");
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out));
  in = g_memory_input_stream_new_from_bytes (bytes);
  packet_out = valent_packet_from_stream (in, -1, NULL, &error);
  g_assert_no_error (error);
  g_clear_object (&out);
  g_clear_object (&in);
  g_clear_pointer (&packet_out, json_node_unref);

  VALENT_TEST_CHECK ("valent_packet_from_stream() sets an error for corrupt packets");
  in = g_memory_input_stream_new_from_data (corrupt_packet, -1, NULL);
  packet_out = valent_packet_from_stream (in, -1, NULL, &error);
  g_assert_error (error, JSON_PARSER_ERROR, JSON_PARSER_ERROR_INVALID_BAREWORD);
  g_clear_object (&in);
  g_clear_pointer (&packet_out, json_node_unref);
  g_clear_error (&error);

  VALENT_TEST_CHECK ("valent_packet_from_stream() sets an error for closed streams");
  in = g_memory_input_stream_new_from_data ("", 0, NULL);
  g_input_stream_close (in, NULL, NULL);
  packet_out = valent_packet_from_stream (in, -1, NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_clear_object (&in);
  g_clear_error (&error);

  VALENT_TEST_CHECK ("valent_packet_from_stream() sets an error for empty streams");
  in = g_memory_input_stream_new_from_data ("", 0, NULL);
  packet_out = valent_packet_from_stream (in, -1, NULL, &error);
  g_assert_error (error, VALENT_PACKET_ERROR, VALENT_PACKET_ERROR_INVALID_DATA);
  g_clear_object (&in);
  g_clear_error (&error);

  VALENT_TEST_CHECK ("valent_packet_from_stream() sets an error for invalid data");
  in = g_memory_input_stream_new_from_data ("\n", 1, NULL);
  packet_out = valent_packet_from_stream (in, -1, NULL, &error);
  g_assert_error (error, VALENT_PACKET_ERROR, VALENT_PACKET_ERROR_INVALID_DATA);
  g_clear_object (&in);
  g_clear_error (&error);

  VALENT_TEST_CHECK ("valent_packet_from_stream() sets an error for oversize packets");
  in = g_memory_input_stream_new_from_data ("1234567890", 10, NULL);
  packet_out = valent_packet_from_stream (in, 5, NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_MESSAGE_TOO_LARGE);
  g_clear_object (&in);
  g_clear_error (&error);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/device/packet/builder",
                   test_packet_builder);

  g_test_add_func ("/libvalent/device/packet/payloads",
                   test_packet_payloads);

  g_test_add_func ("/libvalent/device/packet/get",
                   test_packet_get);

  g_test_add ("/libvalent/device/packet/invalid",
              PacketFixture, NULL,
              packet_fixture_set_up,
              test_packet_invalid,
              packet_fixture_tear_down);

  g_test_add ("/libvalent/device/packet/serializing",
              PacketFixture, NULL,
              packet_fixture_set_up,
              test_packet_serializing,
              packet_fixture_tear_down);

  g_test_add ("/libvalent/device/packet/streaming",
              PacketFixture, NULL,
              packet_fixture_set_up,
              test_packet_streaming,
              packet_fixture_tear_down);

  return g_test_run ();
}
