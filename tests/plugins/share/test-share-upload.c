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
static guint n_test_files = G_N_ELEMENTS (test_files);


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
on_items_changed (GListModel        *model,
                  unsigned int       position,
                  unsigned int       removed,
                  unsigned int       added,
                  ValentTestFixture *fixture)
{
  g_autoptr (ValentTransfer) item = NULL;
  GType item_type;
  unsigned int n_items;

  item = g_list_model_get_item (model, added - 1);
  item_type = g_list_model_get_item_type (model);
  n_items = g_list_model_get_n_items (model);

  g_assert_true (VALENT_IS_TRANSFER (item));
  g_assert_true (item_type  == VALENT_TYPE_TRANSFER);
  g_assert_true (n_items == added);

  g_assert_cmpint (added, ==, n_test_files);
}

static void
test_share_upload_single (ValentTestFixture *fixture,
                          gconstpointer      user_data)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) info = NULL;
  const char *file_name;
  guint32 file_btime_us, file_mtime_us;
  guint64 file_btime_s, file_mtime_s;
  guint64 file_btime, file_mtime;
  goffset file_size;
  JsonNode *packet = NULL;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

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

  file_btime_s = g_file_info_get_attribute_uint64 (info, "time::created");
  file_btime_us = g_file_info_get_attribute_uint32 (info, "time::created-usec");
  file_btime = (file_btime_s * 1000) + floor (file_btime_us / 1000);
  file_mtime_s = g_file_info_get_attribute_uint64 (info, "time::modified");
  file_mtime_us = g_file_info_get_attribute_uint32 (info, "time::modified-usec");
  file_mtime = (file_mtime_s * 1000) + floor (file_mtime_us / 1000);
  file_name = g_file_info_get_name (info);
  file_size = g_file_info_get_size (info);

  transfer = valent_share_upload_new (fixture->device);
  valent_share_upload_add_file (VALENT_SHARE_UPLOAD (transfer), file);
  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)valent_transfer_execute_cb,
                           fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request.update");
  v_assert_packet_cmpint (packet, "numberOfFiles", ==, 1);
  v_assert_packet_cmpint (packet, "totalPayloadSize", ==, file_size);
  g_clear_pointer (&packet, json_node_unref);

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

  valent_test_download (fixture->endpoint, packet, &error);
  g_assert_no_error (error);
  g_clear_pointer (&packet, json_node_unref);
}

static void
test_share_upload_multiple (ValentTestFixture *fixture,
                            gconstpointer      user_data)
{
  g_autoptr (ValentTransfer) transfer = NULL;
  g_autoptr (GListStore) files = NULL;
  g_auto (GStrv) file_name = g_new0 (char *, G_N_ELEMENTS (test_files) + 1);
  goffset file_size[G_N_ELEMENTS (test_files)] = { 0, };
  goffset total_size = 0;
  JsonNode *packet = NULL;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  /* Prepare a list of files */
  files = g_list_store_new (G_TYPE_FILE);
  transfer = valent_share_upload_new (fixture->device);

  for (unsigned int i = 0; i < n_test_files; i++)
    {
      g_autoptr (GFile) file = NULL;
      g_autoptr (GFileInfo) info = NULL;

      file = g_file_new_for_uri (test_files[i]);
      info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_STANDARD_NAME","
                                G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                &error);
      g_assert_no_error (error);

      file_name[i] = g_strdup (g_file_info_get_name (info));
      file_size[i] = g_file_info_get_size (info);
      total_size += g_file_info_get_size (info);

      g_list_store_append (files, file);
    }

  valent_share_upload_add_files (VALENT_SHARE_UPLOAD (transfer),
                                 G_LIST_MODEL (files));
  g_signal_connect (transfer,
                    "items-changed",
                    G_CALLBACK (on_items_changed),
                    fixture);

  valent_transfer_execute (transfer,
                           NULL,
                           (GAsyncReadyCallback)valent_transfer_execute_cb,
                           fixture);

  packet = valent_test_fixture_expect_packet (fixture);
  v_assert_packet_type (packet, "kdeconnect.share.request.update");
  v_assert_packet_cmpint (packet, "numberOfFiles", ==, n_test_files);
  v_assert_packet_cmpint (packet, "totalPayloadSize", ==, total_size);
  json_node_unref (packet);

  for (unsigned int i = 0; i < n_test_files; i++)
    {
      packet = valent_test_fixture_expect_packet (fixture);
      v_assert_packet_type (packet, "kdeconnect.share.request");
      v_assert_packet_cmpstr (packet, "filename", ==, file_name[i]);
      v_assert_packet_field (packet, "creationTime");
      v_assert_packet_field (packet, "lastModified");
      v_assert_packet_cmpint (packet, "numberOfFiles", ==, n_test_files);
      v_assert_packet_cmpint (packet, "totalPayloadSize", ==, total_size);

      g_assert_cmpint (valent_packet_get_payload_size (packet), ==, file_size[i]);

      valent_test_fixture_download (fixture, packet, &error);
      g_assert_no_error (error);

      json_node_unref (packet);
    }

  g_clear_pointer (&file_name, g_strfreev);
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
