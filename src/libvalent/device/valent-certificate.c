// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-certificate"

#include "config.h"

#include <time.h>

#include <gio/gio.h>
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>

#include <libvalent-core.h>
#include "valent-certificate.h"
#include "valent-device.h"

#define ACTIVATION_TIMESPAN (60L*60L*24L*365L)
#define EXPIRATION_TIMESPAN (60L*60L*24L*10L*365L)

/* < private >
 * valent_certificate_generate:
 * @common_name: common name for the certificate
 * @error: (nullable): a `GError`
 *
 * Generate a private key and certificate for @common_name, saving them at
 * @key_path and @cert_path respectively.
 *
 * Returns: (transfer full) (nullable): a new certificate,
 *   or %NULL with @error set
 */
GTlsCertificate *
valent_certificate_generate (const char  *common_name,
                             GError     **error)
{
  gnutls_x509_crt_t crt = NULL;
  gnutls_x509_privkey_t privkey = NULL;
  gnutls_datum_t crt_out = { 0, };
  gnutls_datum_t privkey_out = { 0, };
  g_autofree char *dn = NULL;
  time_t now;
  unsigned char serial[20];
  int rc;
  char *combined_data = NULL;
  size_t combined_size = 0;
  GTlsCertificate *ret = NULL;

  VALENT_ENTRY;

  g_assert (common_name != NULL);

  /* The private key is a 256-bit ECC key. This is `NID_X9_62_prime256v1` in
   * OpenSSL and `GNUTLS_ECC_CURVE_SECP256R1` in GnuTLS.
   */
  if ((rc = gnutls_x509_privkey_init (&privkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_privkey_generate (privkey,
                                          GNUTLS_PK_ECDSA,
                                          GNUTLS_CURVE_TO_BITS (GNUTLS_ECC_CURVE_SECP256R1),
                                          0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_privkey_export2 (privkey,
                                         GNUTLS_X509_FMT_PEM,
                                         &privkey_out)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating private key: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  if ((rc = gnutls_x509_crt_init (&crt)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_set_key (crt, privkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_set_version (crt, 3)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating certificate: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* The certificate is set to be activated 1 year in the past, with an
   * expiration date 10 years in the future.
   */
  now = time (NULL);
  if ((rc = gnutls_x509_crt_set_activation_time (crt, now - ACTIVATION_TIMESPAN)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_set_expiration_time (crt, now + EXPIRATION_TIMESPAN)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Setting certificate activation and expiration: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* While kdeconnect-android uses the static serial `1`, kdeconnect-kde uses
   * a randomized serial, which presumably has some obscure security benefit.
   */
  if ((rc = gnutls_rnd (GNUTLS_RND_RANDOM, serial, sizeof (serial))) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_set_serial (crt, &serial, sizeof (serial))) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Setting certificate serial: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* KDE Connect sets this to `O=KDE,OU=KDE Connect,CN=<device-id>`, where
   * `<device-id>` matches the pattern `/^[a-zA-Z0-9_]{32,38}$/`.
   */
  dn = g_strdup_printf ("O=%s,OU=%s,CN=%s", "Valent", "Valent", common_name);
  if ((rc = gnutls_x509_crt_set_dn (crt, dn, NULL)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Setting certificate common name: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* The signature is a 512-bit SHA512 with ECDSA. This is `EVP_sha512` in
   * OpenSSL and `GNUTLS_DIG_SHA512` in GnuTLS.
   */
  if ((rc = gnutls_x509_crt_sign2 (crt, crt, privkey, GNUTLS_DIG_SHA512, 0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_export2 (crt, GNUTLS_X509_FMT_PEM, &crt_out)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Signing certificate: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Concatenate Private Key and Certificate
   */
  combined_size = privkey_out.size + crt_out.size;
  combined_data = g_malloc0 (combined_size + 1);
  memcpy (combined_data, privkey_out.data, privkey_out.size);
  memcpy (combined_data + privkey_out.size, crt_out.data, crt_out.size);

  ret = g_tls_certificate_new_from_pem (combined_data,
                                        combined_size,
                                        error);

out:
  g_clear_pointer (&crt, gnutls_x509_crt_deinit);
  g_clear_pointer (&privkey, gnutls_x509_privkey_deinit);
  g_clear_pointer (&crt_out.data, gnutls_free);
  g_clear_pointer (&privkey_out.data, gnutls_free);
  g_clear_pointer (&combined_data, g_free);

  VALENT_RETURN (ret);
}

static void
valent_certificate_new_task (GTask        *task,
                             gpointer      source_object,
                             gpointer      task_data,
                             GCancellable *cancellable)
{
  g_autoptr (GTlsCertificate) certificate = NULL;
  const char *path = task_data;
  GError *error = NULL;

  certificate = valent_certificate_new_sync (path, &error);
  if (certificate == NULL)
    {
      g_task_return_error (task, error);
      return;
    }

  g_task_return_pointer (task, g_steal_pointer (&certificate), g_object_unref);
}

/**
 * valent_certificate_new:
 * @path: (type filename) (nullable): a directory path
 * @cancellable: (nullable): `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: user supplied data
 *
 * Get a TLS certificate and private key pair.
 *
 * If @path is given, this function ensures a TLS certificate with the filename
 * `certificate.pem` and private key with filename `private.pem` exist in a
 * directory at @path.
 *
 * Get the result with [func@Valent.certificate_new_finish].
 *
 * Since: 1.0
 */
void
valent_certificate_new (const char          *path,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  g_autoptr (GTask) task = NULL;

  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_certificate_new);
  g_task_set_task_data (task, g_strdup (path), g_free);
  g_task_run_in_thread (task, valent_certificate_new_task);
}

/**
 * valent_certificate_new_finish:
 * @result: a `GAsyncResult` provided to callback
 * @error: (nullable): a `GError`
 *
 * Finish an operation started by [func@Valent.certificate_new].
 *
 * If either generating or loading the certificate failed, %NULL will be
 * returned with @error set.
 *
 * Returns: (transfer full) (nullable): a `GTlsCertificate`
 *
 * Since: 1.0
 */
GTlsCertificate *
valent_certificate_new_finish (GAsyncResult  *result,
                               GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * valent_certificate_new_sync:
 * @path: (type filename) (nullable): a directory path
 * @error: (nullable): a `GError`
 *
 * Get a TLS certificate.
 *
 * If @path is given, this function ensures a TLS certificate with the filename
 * `certificate.pem` and private key with filename `private.pem` exist in a
 * directory at @path.
 *
 * If either generating or loading the certificate fails, %NULL will be returned
 * with @error set.
 *
 * Returns: (transfer full) (nullable): a `GTlsCertificate`
 *
 * Since: 1.0
 */
GTlsCertificate *
valent_certificate_new_sync (const char  *path,
                             GError     **error)
{
  g_autofree char *cert_path = NULL;
  g_autofree char *key_path = NULL;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (path == NULL)
    {
      g_autofree char *cn = NULL;

      cn = valent_device_generate_id ();
      return valent_certificate_generate (cn, error);
    }

  cert_path = g_build_filename (path, "certificate.pem", NULL);
  key_path = g_build_filename (path, "private.pem", NULL);

  if (!g_file_test (cert_path, G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (key_path, G_FILE_TEST_IS_REGULAR))
    {
      g_autoptr (GTlsCertificate) ret = NULL;
      g_autofree char *cn = NULL;
      g_autofree char *certificate_pem = NULL;
      g_autofree char *private_key_pem = NULL;
      gboolean success;

      cn = valent_device_generate_id ();
      ret = valent_certificate_generate (cn, error);
      if (ret == NULL)
        return NULL;

      g_object_get (ret,
                    "certificate-pem", &certificate_pem,
                    "private-key-pem", &private_key_pem,
                    NULL);

      success = g_file_set_contents_full (cert_path,
                                          certificate_pem,
                                          -1,
                                          G_FILE_SET_CONTENTS_DURABLE,
                                          0600,
                                          error);
      if (!success)
        return NULL;

      success = g_file_set_contents_full (key_path,
                                          private_key_pem,
                                          -1,
                                          G_FILE_SET_CONTENTS_DURABLE,
                                          0600,
                                          error);
      if (!success)
        return NULL;

      return g_steal_pointer (&ret);
    }

  return g_tls_certificate_new_from_files (cert_path, key_path, error);
}

/**
 * valent_certificate_get_common_name:
 * @certificate: a `GTlsCertificate`
 *
 * Get the common name from @certificate, which by convention in KDE Connect is
 * the single source of truth for a device's ID.
 *
 * Returns: (transfer none) (nullable): the certificate ID
 *
 * Since: 1.0
 */
const char *
valent_certificate_get_common_name (GTlsCertificate *certificate)
{
  g_autoptr (GByteArray) certificate_der = NULL;
  gnutls_x509_crt_t crt = NULL;
  gnutls_datum_t crt_der = { 0, };
  char buf[64] = { 0, };
  size_t buf_size = 64;
  const char *cn;
  int rc;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  cn = g_object_get_data (G_OBJECT (certificate), "valent-certificate-cn");
  if (cn != NULL)
    return cn;

  g_object_get (certificate, "certificate", &certificate_der, NULL);
  crt_der.data = certificate_der->data;
  crt_der.size = certificate_der->len;

  if ((rc = gnutls_x509_crt_init (&crt)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_import (crt, &crt_der, GNUTLS_X509_FMT_DER)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_get_dn_by_oid (crt,
                                           GNUTLS_OID_X520_COMMON_NAME,
                                           0,
                                           0,
                                           &buf,
                                           &buf_size)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      g_clear_pointer (&crt, gnutls_x509_crt_deinit);

      return NULL;
    }

  /* Intern the common name as private data
   */
  g_object_set_data_full (G_OBJECT (certificate),
                          "valent-certificate-cn",
                          g_strndup (buf, buf_size),
                          g_free);
  g_clear_pointer (&crt, gnutls_x509_crt_deinit);

  return g_object_get_data (G_OBJECT (certificate), "valent-certificate-cn");
}

/**
 * valent_certificate_get_public_key:
 * @certificate: a `GTlsCertificate`
 *
 * Get the public key of @certificate.
 *
 * Returns: (transfer none): a DER-encoded public key
 *
 * Since: 1.0
 */
GByteArray *
valent_certificate_get_public_key (GTlsCertificate *certificate)
{
  g_autoptr (GByteArray) certificate_der = NULL;
  g_autoptr (GByteArray) pk = NULL;
  gnutls_x509_crt_t crt = NULL;
  gnutls_datum_t crt_der = { 0, };
  gnutls_pubkey_t pubkey = NULL;
  size_t size;
  int rc;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  pk = g_object_get_data (G_OBJECT (certificate), "valent-certificate-pk");
  if (pk != NULL)
    return g_steal_pointer (&pk);

  g_object_get (certificate, "certificate", &certificate_der, NULL);
  crt_der.data = certificate_der->data;
  crt_der.size = certificate_der->len;

  if ((rc = gnutls_x509_crt_init (&crt)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_import (crt, &crt_der, GNUTLS_X509_FMT_DER)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  if ((rc = gnutls_pubkey_init (&pubkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_pubkey_import_x509 (pubkey, crt, 0)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* First call to get the size, since GByteArray.len is an `unsigned int`,
   * while the output is a `size_t`.
   */
  rc = gnutls_pubkey_export (pubkey, GNUTLS_X509_FMT_DER, NULL, &size);
  if (rc != GNUTLS_E_SUCCESS && rc != GNUTLS_E_SHORT_MEMORY_BUFFER)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  g_assert (size <= (size_t)UINT_MAX);
  pk = g_byte_array_sized_new (size);
  pk->len = (unsigned int)size;
  if ((rc = gnutls_pubkey_export (pubkey,
                                  GNUTLS_X509_FMT_DER,
                                  pk->data, &size)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Intern the DER as private data
   */
  g_object_set_data_full (G_OBJECT (certificate),
                          "valent-certificate-pk",
                          g_steal_pointer (&pk),
                          (GDestroyNotify)g_byte_array_unref);

out:
  g_clear_pointer (&crt, gnutls_x509_crt_deinit);
  g_clear_pointer (&pubkey, gnutls_pubkey_deinit);

  return g_object_get_data (G_OBJECT (certificate), "valent-certificate-pk");
}

