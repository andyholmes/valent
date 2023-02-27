// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>


static GTlsCertificate *certificate = NULL;


static void
new_certificate_cb (GObject      *object,
                    GAsyncResult *result,
                    GMainLoop    *loop)
{
  GError *error = NULL;

  certificate = valent_certificate_new_finish (result, &error);
  g_assert_no_error (error);
  g_main_loop_quit (loop);
}

static void
test_certificate_new (void)
{
  g_autoptr (GMainLoop) loop = NULL;
  g_autofree char *path = NULL;

  loop = g_main_loop_new (NULL, FALSE);
  path = g_dir_make_tmp ("XXXXXX.valent", NULL);

  valent_certificate_new (path,
                          NULL,
                          (GAsyncReadyCallback)new_certificate_cb,
                          loop);
  g_main_loop_run (loop);
  g_assert_true (G_IS_TLS_CERTIFICATE (certificate));
  g_clear_object (&certificate);
}

static void
test_certificate_properties (void)
{
  g_autofree char *path = NULL;
  const char *common_name = NULL;
  const char *fingerprint = NULL;
  GByteArray *public_key = NULL;

  path = g_dir_make_tmp ("XXXXXX.valent", NULL);

  certificate = valent_certificate_new_sync (path, NULL);
  g_assert_true (G_IS_TLS_CERTIFICATE (certificate));

  common_name = valent_certificate_get_common_name (certificate);
  g_assert_true (common_name != NULL && *common_name != '\0');

  fingerprint = valent_certificate_get_fingerprint (certificate);
  g_assert_true (fingerprint != NULL && *fingerprint != '\0');

  public_key = valent_certificate_get_public_key (certificate);
  g_assert_true (public_key != NULL);
  g_assert_cmpuint (public_key->len, >, 0);

  g_clear_object (&certificate);
}

int
main (int   argc,
      char *argv[])
{
  valent_test_init (&argc, &argv, NULL);

  g_test_add_func ("/libvalent/core/certificate/new",
                   test_certificate_new);
  g_test_add_func ("/libvalent/core/certificate/properties",
                   test_certificate_properties);

  return g_test_run ();
}
