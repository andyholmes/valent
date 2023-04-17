// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-share-download.h"


static const char *test_file = "resource:///tests/image.png";

static void
test_share_download_single (ValentTestFixture *fixture,
                            gconstpointer      user_data)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) dest = NULL;
  const char *dest_dir = NULL;
  JsonNode *packet = NULL;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  /* Ensure the download directory is at it's default */
  g_settings_reset (fixture->settings, "download-folder");

  file = g_file_new_for_uri (test_file);
  packet = valent_test_fixture_lookup_packet (fixture, "share-file");

  valent_test_upload (fixture->endpoint, packet, file, &error);
  g_assert_no_error (error);

  /* Ensure the download task has an opportunity to finish completely */
  valent_test_await_timeout (1);

  dest_dir = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
  dest = valent_get_user_file (dest_dir, "image.png", FALSE);
  g_assert_true (g_file_query_exists (dest, NULL));
}

static void
test_share_download_multiple (ValentTestFixture *fixture,
                              gconstpointer      user_data)
{
  g_autoptr (GFile) file = NULL;
  g_autoptr (GFile) dest = NULL;
  const char *dest_dir = NULL;
  JsonNode *packet = NULL;
  GError *error = NULL;

  valent_test_fixture_connect (fixture, TRUE);

  /* Ensure the download directory is at it's default */
  g_settings_reset (fixture->settings, "download-folder");

  file = g_file_new_for_uri (test_file);

  /* The first packet indicates two files will be transferred */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-1");
  valent_test_upload (fixture->endpoint, packet, file, &error);
  g_assert_no_error (error);

  /* The update packet indicates a third file has been queued */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-2");
  valent_test_fixture_handle_packet (fixture, packet);

  /* The second payload indicates three files will be transferred */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-3");
  valent_test_upload (fixture->endpoint, packet, file, &error);
  g_assert_no_error (error);

  /* The third payload indicates three files will be transferred */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-4");
  valent_test_upload (fixture->endpoint, packet, file, &error);
  g_assert_no_error (error);

  /* Check the received files */
  dest_dir = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);

  dest = valent_get_user_file (dest_dir, "image.png", FALSE);
  g_assert_true (g_file_query_exists (dest, NULL));
  g_clear_object (&dest);

  dest = valent_get_user_file (dest_dir, "image.png (1)", FALSE);
  g_assert_true (g_file_query_exists (dest, NULL));
  g_clear_object (&dest);

  dest = valent_get_user_file (dest_dir, "image.png (2)", FALSE);
  g_assert_true (g_file_query_exists (dest, NULL));
  g_clear_object (&dest);
}

int
main (int   argc,
      char *argv[])
{
  const char *path = "plugin-share.json";

  valent_test_init (&argc, &argv, NULL);

  g_test_add ("/plugins/share/download-single",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_download_single,
              valent_test_fixture_clear);

  g_test_add ("/plugins/share/download-multiple",
              ValentTestFixture, path,
              valent_test_fixture_init,
              test_share_download_multiple,
              valent_test_fixture_clear);

  return g_test_run ();
}
