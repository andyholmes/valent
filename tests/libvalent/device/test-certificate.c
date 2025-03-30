// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <valent.h>
#include <libvalent-test.h>


static void
new_certificate_cb (GObject          *object,
                    GAsyncResult     *result,
                    GTlsCertificate **certificate)
{
  GError *error = NULL;

  if (certificate != NULL)
    *certificate = valent_certificate_new_finish (result, &error);

  g_assert_no_error (error);
}

static void
test_certificate_new (void)
{
  g_autoptr (GTlsCertificate) generated = NULL;
  g_autoptr (GTlsCertificate) loaded = NULL;
  g_autoptr (GTlsCertificate) memory = NULL;
  g_autofree char *path = NULL;
  g_autoptr (GFile) cert = NULL;
  g_autoptr (GFile) privkey = NULL;

  path = g_dir_make_tmp ("XXXXXX.valent", NULL);
  cert = g_file_new_build_filename (path, "certificate.pem", NULL);
  privkey = g_file_new_build_filename (path, "private.pem", NULL);

  g_assert_false (g_file_query_exists (cert, NULL));
  g_assert_false (g_file_query_exists (privkey, NULL));

  VALENT_TEST_CHECK ("A certificate can be generated for a path");
  valent_certificate_new (path,
                          NULL,
                          (GAsyncReadyCallback)new_certificate_cb,
                          &generated);
  valent_test_await_pointer (&generated);
  g_assert_true (G_IS_TLS_CERTIFICATE (generated));
  g_assert_true (g_file_query_exists (cert, NULL));
  g_assert_true (g_file_query_exists (privkey, NULL));

  VALENT_TEST_CHECK ("A certificate can be loaded from a path");
  valent_certificate_new (path,
                          NULL,
                          (GAsyncReadyCallback)new_certificate_cb,
                          &loaded);
  valent_test_await_pointer (&loaded);
  g_assert_true (G_IS_TLS_CERTIFICATE (loaded));
  g_assert_true (g_tls_certificate_is_same (loaded, generated));

  VALENT_TEST_CHECK ("A certificate can be generated in-memory");
  valent_certificate_new (NULL,
                          NULL,
                          (GAsyncReadyCallback)new_certificate_cb,
                          &memory);
  valent_test_await_pointer (&memory);
  g_assert_true (G_IS_TLS_CERTIFICATE (memory));
}

static void
test_certificate_properties (void)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  const char *common_name = NULL;
  GByteArray *public_key = NULL;

  certificate = valent_certificate_new_sync (NULL, NULL);
  g_assert_true (G_IS_TLS_CERTIFICATE (certificate));

  common_name = valent_certificate_get_common_name (certificate);
  g_assert_true (common_name != NULL && *common_name != '\0');

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

  g_test_add_func ("/libvalent/device/certificate/new",
                   test_certificate_new);
  g_test_add_func ("/libvalent/device/certificate/properties",
                   test_certificate_properties);

  return g_test_run ();
}
