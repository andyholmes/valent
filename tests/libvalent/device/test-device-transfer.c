// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <math.h>

#include <gio/gio.h>
#include <valent.h>
#include <libvalent-test.h>


static void
test_device_transfer (ValentTestFixture *fixture,
                      gconstpointer      user_data)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFileInfo) src_info = NULL;
  g_autoptr (GFile) dest = NULL;
  g_autoptr (GFileInfo) dest_info = NULL;
  const char *dest_dir = NULL;
  JsonNode *packet = NULL;
  guint64 src_btime_s, src_mtime_s, dest_mtime_s;
  guint32 src_btime_us, src_mtime_us, dest_mtime_us;
  guint64 src_btime, src_mtime, dest_mtime;
  goffset src_size, dest_size;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  file = g_file_new_for_uri ("resource:///tests/image.png");
  src_info = g_file_query_info (file,
                                G_FILE_ATTRIBUTE_TIME_CREATED","
                                G_FILE_ATTRIBUTE_TIME_CREATED_USEC","
                                G_FILE_ATTRIBUTE_TIME_MODIFIED","
                                G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC","
                                G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                &error);
  g_assert_no_error (error);

  src_btime_s = g_file_info_get_attribute_uint64 (src_info, "time::created");
  src_btime_us = g_file_info_get_attribute_uint32 (src_info, "time::created-usec");
  src_btime = (src_btime_s * 1000) + floor (src_btime_us / 1000);
  src_mtime_s = g_file_info_get_attribute_uint64 (src_info, "time::modified");
  src_mtime_us = g_file_info_get_attribute_uint32 (src_info, "time::modified-usec");
  src_mtime = (src_mtime_s * 1000) + floor (src_mtime_us / 1000);
  src_size = g_file_info_get_size (src_info);

  packet = valent_test_fixture_lookup_packet (fixture, "test-transfer");
  json_object_set_int_member (valent_packet_get_body (packet),
                              "creationTime",
                              src_btime);
  json_object_set_int_member (valent_packet_get_body (packet),
                              "lastModified",
                              src_mtime);

  valent_test_upload (fixture->endpoint, packet, file, &error);
  g_assert_no_error (error);

  /* Ensure the download task has time to set the file mtime */
  valent_test_wait (1);

  dest_dir = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
  dest = valent_get_user_file (dest_dir, "image.png", FALSE);
  dest_info = g_file_query_info (dest,
                                 G_FILE_ATTRIBUTE_TIME_MODIFIED","
                                 G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC","
                                 G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                 G_FILE_QUERY_INFO_NONE,
                                 NULL,
                                 &error);
  g_assert_no_error (error);

  /* FIXME: Setting mtime doesn't work in flatpak or CI */
  if (FALSE)
    {
      /* NOTE: we're not checking the btime, because the Linux kernel doesn't
       *       support setting it... */
      dest_mtime_s = g_file_info_get_attribute_uint64 (dest_info, "time::modified");
      dest_mtime_us = g_file_info_get_attribute_uint32 (dest_info, "time::modified-usec");
      dest_mtime = (dest_mtime_s * 1000) + floor (dest_mtime_us / 1000);
      dest_size = g_file_info_get_size (dest_info);

      g_assert_cmpuint (src_mtime, ==, dest_mtime);
      g_assert_cmpuint (src_size, ==, dest_size);
    }
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "core.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/libvalent/device/device-transfer",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_device_transfer,
              valent_test_fixture_clear);

  return g_test_run ();
}
