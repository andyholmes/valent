// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <math.h>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-share-upload.h"


static const char * const test_files[] = {
    "resource:///tests/image.png",
    "resource:///tests/contact.vcf",
    "resource:///tests/contact2.vcf",
    "resource:///tests/contact3.vcf",
};


static void
valent_transfer_execute_cb (ValentTransfer    *transfer,
                            GAsyncResult      *result,
                            ValentTestFixture *fixture)
{
  GError *error = NULL;

  g_assert_true (valent_transfer_execute_finish (transfer, result, &error));
  g_assert_no_error (error);
}

static void
on_items_changed (GListModel   *model,
                  unsigned int  position,
                  unsigned int  removed,
                  unsigned int  added,
                  gpointer      user_data)
{
  if (g_list_model_get_n_items (model) == G_N_ELEMENTS (test_files))
    valent_test_quit_loop ();
}

static void
test_share_upload_single (ValentTestFixture *fixture,
                          gconstpointer      user_data)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) info = NULL;
  const char *file_name;
  uint32_t file_btime_us, file_mtime_us;
  uint64_t file_btime_s, file_mtime_s;
  int64_t file_btime, file_mtime;
  goffset file_size;
  JsonNode *packet = NULL;
  GError *error = NULL;

  valent_test_fixture_connect (fixture);

  file = g_file_new_for_uri (test_files[0]);
  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_TIME_CREATED","
                            G_FILE_ATTRIBUTE_TIME_CREATED_USEC","
                            G_FILE_ATTRIBUTE_TIME_MODIFIED","
                            G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC","
                            G_FILE_ATTRIBUTE_STANDARD_NAME","
                            G_FILE_ATTRIBUTE_STANDARD_SIZE,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  g_assert_no_error (error);

  file_name = g_file_info_get_name (info);
  file_btime_s = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CREATED);
  file_btime_us = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_CREATED_USEC);
  file_btime = (int64_t)((file_btime_s * 1000) + floor (file_btime_us / 1000));
  file_mtime_s = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  file_mtime_us = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC);
  file_mtime = (int64_t)((file_mtime_s * 1000) + floor (file_mtime_us / 1000));
  file_size = g_file_info_get_size (info);

  VALENT_TEST_CHECK ("Transfer can send a single file");
  transfer = valent_share_upload_new (fixture->device);
  valent_share_upload_add_file (VALENT_SHARE_UPLOAD (transfer), file);
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)valent_transfer_execute_cb,
                           fixture);

  VALENT_TEST_CHECK ("Transfer sends updates for queued files");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request.update");
  v_assert_packet_cmpint (packet, "numberOfFiles", ==, 1);
  v_assert_packet_cmpint (packet, "totalPayloadSize", ==, file_size);
  g_clear_pointer (&packet, json_node_unref);

  VALENT_TEST_CHECK ("Transfer sends payload for queued files");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request");
  v_assert_packet_cmpstr (packet, "filename", ==, file_name);
  v_assert_packet_cmpint (packet, "creationTime", ==, file_btime);
  v_assert_packet_cmpint (packet, "numberOfFiles", ==, 1);
  v_assert_packet_cmpint (packet, "totalPayloadSize", ==, file_size);

  /* TODO: Setting mtime in flatpak or CI doesn't work */
  if (FALSE)
    v_assert_packet_cmpint (packet, "lastModified", ==, file_mtime);
  else
    v_assert_packet_field (packet, "lastModified");

  g_assert_cmpint (valent_packet_get_payload_size (packet), ==, file_size);

  valent_test_fixture_download (fixture, packet, &error);
  g_assert_no_error (error);
  g_clear_pointer (&packet, json_node_unref);
}

static void
test_share_upload_multiple (ValentTestFixture *fixture,
                            gconstpointer      user_data)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  goffset total_size = 0;
  goffset received_size = 0;
  size_t received_files = 0;
  unsigned int n_items;
  JsonNode *packet = NULL;
  GError *error = NULL;

  valent_test_fixture_connect (fixture);

  VALENT_TEST_CHECK ("Transfer can queue multiple files");
  transfer = valent_share_upload_new (fixture->device);
  for (size_t i = 0; i < G_N_ELEMENTS (test_files); i++)
    {
      g_autoptr (GFile) file = NULL;
      g_autoptr (GFileInfo) info = NULL;

      file = g_file_new_for_uri (test_files[i]);
      info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                &error);
      g_assert_no_error (error);

      total_size += g_file_info_get_size (info);
      valent_share_upload_add_file (VALENT_SHARE_UPLOAD (transfer), file);
    }

  g_signal_connect (transfer,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    NULL);
  valent_test_run_loop ();

  VALENT_TEST_CHECK ("Transfer can send multiple files");
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)valent_transfer_execute_cb,
                           fixture);

  VALENT_TEST_CHECK ("Transfer sends updates for queued files");
  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request.update");
  v_assert_packet_cmpint (packet, "numberOfFiles", <=, G_N_ELEMENTS (test_files));
  v_assert_packet_cmpint (packet, "totalPayloadSize", <=, total_size);
  json_node_unref (packet);

  while (received_files < G_N_ELEMENTS (test_files))
    {
      packet = valent_test_fixture_expect_packet (fixture);
      v_assert_packet_cmpint (packet, "numberOfFiles", <=, G_N_ELEMENTS (test_files));
      v_assert_packet_cmpint (packet, "totalPayloadSize", <=, total_size);

      if (valent_packet_has_payload (packet))
        {
          received_files += 1;
          received_size += valent_packet_get_payload_size (packet);

          v_assert_packet_type (packet, "kdeconnect.share.request");
          v_assert_packet_field (packet, "filename");
          v_assert_packet_field (packet, "creationTime");
          v_assert_packet_field (packet, "lastModified");

          valent_test_fixture_download (fixture, packet, &error);
          g_assert_no_error (error);
        }

      json_node_unref (packet);
    }

  g_assert_cmpint (total_size, ==, received_size);
  g_assert_cmpint (G_N_ELEMENTS (test_files), ==, received_files);

  VALENT_TEST_CHECK ("Transfer implements GListModel correctly");
  g_assert_true (G_LIST_MODEL (transfer));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (transfer)), >, 0);
  g_assert_true (g_list_model_get_item_type (G_LIST_MODEL (transfer)) == VALENT_TYPE_TRANSFER);

  n_items = g_list_model_get_n_items (G_LIST_MODEL (transfer));
  for (unsigned int i = 0; i < n_items; i++)
    {
      g_autoptr (GListModel) item = g_list_model_get_item (G_LIST_MODEL (transfer), i);
      g_assert_true (VALENT_IS_TRANSFER (item));
    }
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-share.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/share/upload-single",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_upload_single,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/upload-multiple",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_upload_multiple,
              valent_test_fixture_clear);

  return g_test_run ();
}
