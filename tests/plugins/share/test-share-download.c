// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>

#include "valent-share-download.h"


static const char *test_file = "resource:///tests/image.png";
static const char *file_names[] = {
  "kittens.png",
  "puppies.png",
  "puppies-and-kittens.png",
};

static void
on_changed (GFileMonitor      *monitor,
            GFile             *file,
            GFile             *other_file,
            GFileMonitorEvent  event_type,
            gpointer           user_data)
{
  unsigned int *pending = (unsigned int *)user_data;

  if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
    {
      g_autofree char *basename = NULL;

      basename = g_file_get_basename (file);
      if (g_str_has_suffix (basename, ".png"))
        *pending -= 1;
    }
}

static void
test_share_download_single (ValentTestFixture *fixture,
                            gconstpointer      user_data)
{
  g_autoptr (GFileMonitor) monitor = NULL;
  g_autoptr (GFile) source = NULL;
  g_autoptr (GFile) dest_dir = NULL;
  g_autoptr (GFile) dest = NULL;
  const char *downloads_path = NULL;
  JsonNode *packet = NULL;
  GError *error = NULL;
  unsigned int pending = 1;

  valent_test_fixture_connect (fixture);

  /* Ensure the download directory is at it's default
   * and monitor the destination for new files
   */
  g_settings_reset (fixture->settings, "download-folder");
  downloads_path = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
  dest_dir = g_file_new_for_path (downloads_path);
  monitor = g_file_monitor_directory (dest_dir,
                                      G_FILE_MONITOR_WATCH_MOVES,
                                      NULL,
                                      NULL);
  g_signal_connect (monitor,
                    "changed",
                    G_CALLBACK (on_changed),
                    &pending);

  source = g_file_new_for_uri (test_file);
  packet = valent_test_fixture_lookup_packet (fixture, "share-file");
  valent_test_fixture_upload (fixture, packet, source, &error);
  g_assert_no_error (error);

  /* Ensure the download task has an opportunity to finish completely
   */
  while (pending > 0)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Single files are saved at the expected path");
  dest = valent_get_user_file (downloads_path, "image.png", FALSE);
  g_assert_true (g_file_query_exists (dest, NULL));
}

static void
test_share_download_multiple (ValentTestFixture *fixture,
                              gconstpointer      user_data)
{
  g_autoptr (GFileMonitor) monitor = NULL;
  g_autoptr (GFile) source = NULL;
  g_autoptr (GFile) dest_dir = NULL;
  const char *downloads_path = NULL;
  JsonNode *packet = NULL;
  unsigned int pending = 3;
  GError *error = NULL;

  valent_test_fixture_connect (fixture);

  /* Ensure the download directory is at it's default
   * and monitor the destination for new files
   */
  g_settings_reset (fixture->settings, "download-folder");
  downloads_path = valent_get_user_directory (G_USER_DIRECTORY_DOWNLOAD);
  dest_dir = g_file_new_for_path (downloads_path);
  monitor = g_file_monitor_directory (dest_dir,
                                      G_FILE_MONITOR_WATCH_MOVES,
                                      NULL,
                                      NULL);
  g_signal_connect (monitor,
                    "changed",
                    G_CALLBACK (on_changed),
                    &pending);

  source = g_file_new_for_uri (test_file);

  /* The first packet indicates two files will be transferred, and carries
   * transfer info for the first payload
   */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-1");
  valent_test_fixture_upload (fixture, packet, source, &error);
  g_assert_no_error (error);

  /* The second packet is an update indicating a third file has been queued
   */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-2");
  valent_test_fixture_handle_packet (fixture, packet);

  /* The third packet indicates three files will be transferred, and carries
   * transfer info for the second payload
   */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-3");
  valent_test_fixture_upload (fixture, packet, source, &error);
  g_assert_no_error (error);

  /* The fourth packet indicates three files will be transferred, and carries
   * transfer info for the third payload
   */
  packet = valent_test_fixture_lookup_packet (fixture, "share-multiple-4");
  valent_test_fixture_upload (fixture, packet, source, &error);

  /* Ensure the download task has an opportunity to finish completely
   */
  while (pending > 0)
    g_main_context_iteration (NULL, FALSE);

  VALENT_TEST_CHECK ("Multiple files are saved at the expected paths");
  for (size_t i = 0; i < G_N_ELEMENTS (file_names); i++)
    {
      g_autoptr (GFile) dest = NULL;

      dest = valent_get_user_file (downloads_path, file_names[i], FALSE);
      g_assert_true (g_file_query_exists (dest, NULL));
    }
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
