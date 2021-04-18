#include <libvalent-core.h>


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
}

static void
test_packet_serializing (void)
{
  JsonBuilder *builder;
  JsonNode *packet;
  char *packet_str;

  builder = valent_packet_start ("kdeconnect.mock");
  packet = valent_packet_finish (builder);

  packet_str = valent_packet_serialize (packet);
  g_assert_nonnull (packet_str);
  json_node_unref (packet);

  packet = valent_packet_deserialize (packet_str, NULL);
  g_assert_true (valent_packet_is_valid (packet));
  json_node_unref (packet);

  g_free (packet_str);
}

static void
test_packet_streaming (void)
{
  JsonBuilder *builder;
  JsonNode *packet;
  g_autoptr (GError) error = NULL;
  g_autoptr (GInputStream) in = NULL;
  g_autoptr (GOutputStream) out = NULL;
  g_autoptr (GBytes) bytes = NULL;

  builder = valent_packet_start ("kdeconnect.mock");
  packet = valent_packet_finish (builder);

  out = g_memory_output_stream_new_resizable ();
  valent_packet_to_stream (out, packet, NULL, &error);
  g_assert_no_error (error);
  json_node_unref (packet);

  g_output_stream_close (out, NULL, &error);
  g_assert_no_error (error);
  bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (out));

  in = g_memory_input_stream_new_from_bytes (bytes);
  packet = valent_packet_from_stream (in, NULL, &error);
  g_assert_no_error (error);
  json_node_unref (packet);
}

static void
test_packet_payloads (void)
{
  JsonBuilder *builder;
  g_autoptr (JsonNode) packet = NULL;
  JsonNode *transfer_info;
  JsonObject *root;

  /* Build a mock packet with payload */
  builder = valent_packet_start ("kdeconnect.mock.transfer");
  packet = valent_packet_finish (builder);

  builder = json_builder_new ();
  json_builder_begin_object (builder);
  json_builder_set_member_name (builder, "port");
  json_builder_add_int_value (builder, 1739);
  json_builder_end_object (builder);
  transfer_info = json_builder_get_root (builder);

  root = json_node_get_object (packet);
  json_object_set_int_member (root, "payloadSize", 42);
  json_object_set_member (root, "payloadTransferInfo", transfer_info);
  g_object_unref (builder);

  /* Tests */
  g_assert_true (valent_packet_has_payload (packet));
  g_assert_cmpint (valent_packet_get_payload_size (packet), ==, 42);

  valent_packet_set_payload_size (packet, 84);
  g_assert_cmpint (valent_packet_get_payload_size (packet), ==, 84);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/core/packet/builder",
                   test_packet_builder);

  g_test_add_func ("/core/packet/serializing",
                   test_packet_serializing);

  g_test_add_func ("/core/packet/streaming",
                   test_packet_streaming);

  g_test_add_func ("/core/packet/payloads",
                   test_packet_payloads);

  return g_test_run ();
}
