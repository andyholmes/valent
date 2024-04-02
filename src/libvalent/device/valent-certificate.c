// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-certificate"

#include "config.h"

#include <time.h>

#include <gio/gio.h>
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>

#include <libvalent-core.h>
#include "valent-certificate.h"

#define DEFAULT_EXPIRATION (60L*60L*24L*10L*365L)
#define DEFAULT_KEY_SIZE   4096

#define SHA256_HEX_LEN 64
#define SHA256_STR_LEN 96


/**
 * valent_certificate_generate:
 * @cert_path: (type filename): file path to the certificate
 * @key_path: (type filename): file path to the private key
 * @common_name: common name for the certificate
 * @error: (nullable): a `GError`
 *
 * Generate a private key and certificate for @common_name, saving them at
 * @key_path and @cert_path respectively.
 *
 * Returns: %TRUE if successful, or %FALSE with @error set
 *
 * Since: 1.0
 */
static gboolean
valent_certificate_generate (const char  *cert_path,
                             const char  *key_path,
                             const char  *common_name,
                             GError     **error)
{
  g_autofree char *dn = NULL;
  gnutls_x509_privkey_t privkey = NULL;
  gnutls_x509_crt_t crt = NULL;
  gnutls_datum_t out;
  time_t timestamp;
  unsigned int serial;
  int rc;
  gboolean ret = FALSE;

  VALENT_ENTRY;

  /*
   * Private Key
   */
  if ((rc = gnutls_x509_privkey_init (&privkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_privkey_generate (privkey,
                                          GNUTLS_PK_RSA,
                                          DEFAULT_KEY_SIZE,
                                          0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_privkey_export2 (privkey,
                                         GNUTLS_X509_FMT_PEM,
                                         &out)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating private key: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Output the private key PEM to file */
  ret = g_file_set_contents_full (key_path,
                                  (const char *)out.data,
                                  out.size,
                                  G_FILE_SET_CONTENTS_DURABLE,
                                  0600,
                                  error);
  gnutls_free (out.data);

  if (!ret)
    VALENT_GOTO (out);

  /*
   * TLS Certificate
   */
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

  /* Expiry (10 years) */
  timestamp = time (NULL);

  if ((rc = gnutls_x509_crt_set_activation_time (crt, timestamp)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_set_expiration_time (crt, timestamp + DEFAULT_EXPIRATION)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating certificate: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Serial Number */
  serial = GUINT32_TO_BE (10);
  gnutls_x509_crt_set_serial (crt, &serial, sizeof (unsigned int));

  /* Distinguished Name (RFC4514) */
  dn = g_strdup_printf ("O=%s,OU=%s,CN=%s", "Valent", "Valent", common_name);

  if ((rc = gnutls_x509_crt_set_dn (crt, dn, NULL)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Generating certificate: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Sign and export the certificate */
  if ((rc = gnutls_x509_crt_sign2 (crt, crt, privkey, GNUTLS_DIG_SHA256, 0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_export2 (crt, GNUTLS_X509_FMT_PEM, &out)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Signing certificate: %s",
                   gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Output the certificate PEM to file */
  ret = g_file_set_contents_full (cert_path,
                                  (const char *)out.data,
                                  out.size,
                                  G_FILE_SET_CONTENTS_DURABLE,
                                  0600,
                                  error);
  gnutls_free (out.data);

  out:
    gnutls_x509_crt_deinit (crt);
    gnutls_x509_privkey_deinit (privkey);

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

  if ((certificate = valent_certificate_new_sync (path, &error)) == NULL)
    return g_task_return_error (task, error);

  g_task_return_pointer (task, g_steal_pointer (&certificate), g_object_unref);
}

/**
 * valent_certificate_new:
 * @path: (type filename): a directory path
 * @cancellable: (nullable): `GCancellable`
 * @callback: (scope async): a `GAsyncReadyCallback`
 * @user_data: (closure): user supplied data
 *
 * Get a TLS certificate and private key pair.
 *
 * This ensures a TLS certificate with the filename `certificate.pem` and
 * private key with filename `private.pem` exist in a directory at @path.
 *
 * If either one doesn't exist, a new certificate and private key pair will be
 * generated. The common name will be set to a string returned by
 * [func@GLib.uuid_string_random].
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

  g_return_if_fail (path != NULL && *path != '\0');
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
 * @path: (type filename): a directory path
 * @error: (nullable): a `GError`
 *
 * Get a TLS certificate and private key pair.
 *
 * This ensures a TLS certificate with the filename `certificate.pem` and
 * private key with filename `private.pem` exist in a directory at @path.
 *
 * If either one doesn't exist, a new certificate and private key pair will be
 * generated. The common name will be set to a string returned by
 * [func@GLib.uuid_string_random].
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

  g_return_val_if_fail (path != NULL && *path != '\0', NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  cert_path = g_build_filename (path, "certificate.pem", NULL);
  key_path = g_build_filename (path, "private.pem", NULL);

  if (!g_file_test (cert_path, G_FILE_TEST_IS_REGULAR) ||
      !g_file_test (key_path, G_FILE_TEST_IS_REGULAR))
    {
      g_autofree char *cn = NULL;

      cn = g_uuid_string_random ();

      if (!valent_certificate_generate (cert_path, key_path, cn, error))
        return FALSE;
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
  gnutls_x509_crt_t crt;
  gnutls_datum_t crt_der;
  char buf[64] = { 0, };
  size_t buf_size = 64;
  const char *device_id;
  int rc;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  /* Check */
  device_id = g_object_get_data (G_OBJECT (certificate),
                                 "valent-certificate-cn");

  if G_LIKELY (device_id != NULL)
    return device_id;

  /* Extract the common name */
  g_object_get (certificate, "certificate", &certificate_der, NULL);
  crt_der.data = certificate_der->data;
  crt_der.size = certificate_der->len;

  if ((rc = gnutls_x509_crt_init (&crt)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_import (crt, &crt_der, 0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_get_dn_by_oid (crt,
                                           GNUTLS_OID_X520_COMMON_NAME,
                                           0,
                                           0,
                                           &buf,
                                           &buf_size)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      gnutls_x509_crt_deinit (crt);

      return NULL;
    }

  gnutls_x509_crt_deinit (crt);

  /* Intern the id as private data */
  g_object_set_data_full (G_OBJECT (certificate),
                          "valent-certificate-cn",
                          g_strndup (buf, buf_size),
                          g_free);

  return g_object_get_data (G_OBJECT (certificate), "valent-certificate-cn");
}

/**
 * valent_certificate_get_fingerprint:
 * @certificate: a `GTlsCertificate`
 *
 * Get a SHA256 fingerprint hash of @certificate.
 *
 * Returns: (transfer none): a SHA256 hash
 *
 * Since: 1.0
 */
const char *
valent_certificate_get_fingerprint (GTlsCertificate *certificate)
{
  g_autoptr (GByteArray) certificate_der = NULL;
  g_autoptr (GChecksum) checksum = NULL;
  const char *check;
  const char *fingerprint;
  char buf[SHA256_STR_LEN] = { 0, };
  unsigned int i = 0;
  unsigned int o = 0;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  fingerprint = g_object_get_data (G_OBJECT (certificate),
                                   "valent-certificate-fp");

  if G_LIKELY (fingerprint != NULL)
    return fingerprint;

  g_object_get (certificate, "certificate", &certificate_der, NULL);
  checksum = g_checksum_new (G_CHECKSUM_SHA256);
  g_checksum_update (checksum, certificate_der->data, certificate_der->len);

  check = g_checksum_get_string (checksum);

  while (i < SHA256_HEX_LEN)
    {
      buf[o++] = check[i++];
      buf[o++] = check[i++];
      buf[o++] = ':';
    }
  buf[SHA256_STR_LEN - 1] = '\0';

  /* Intern the hash as private data */
  g_object_set_data_full (G_OBJECT (certificate),
                          "valent-certificate-fp",
                          g_strdup (buf),
                          g_free);

  return g_object_get_data (G_OBJECT (certificate), "valent-certificate-fp");
}

/**
 * valent_certificate_get_public_key:
 * @certificate: a `GTlsCertificate`
 *
 * Get the public key of @certificate.
 *
 * Returns: (transfer none): a DER-encoded publickey
 *
 * Since: 1.0
 */
GByteArray *
valent_certificate_get_public_key (GTlsCertificate *certificate)
{
  g_autoptr (GByteArray) certificate_der = NULL;
  g_autoptr (GByteArray) pubkey = NULL;
  size_t size;
  gnutls_x509_crt_t crt = NULL;
  gnutls_datum_t crt_der;
  gnutls_pubkey_t crt_pk = NULL;
  int rc;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  pubkey = g_object_get_data (G_OBJECT (certificate),
                              "valent-certificate-pk");

  if (pubkey != NULL)
    return g_steal_pointer (&pubkey);

  g_object_get (certificate, "certificate", &certificate_der, NULL);
  crt_der.data = certificate_der->data;
  crt_der.size = certificate_der->len;

  /* Load the certificate */
  if ((rc = gnutls_x509_crt_init (&crt)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_import (crt, &crt_der, GNUTLS_X509_FMT_DER)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Load the public key */
  if ((rc = gnutls_pubkey_init (&crt_pk)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_pubkey_import_x509 (crt_pk, crt, 0)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
      VALENT_GOTO (out);
    }

  /* Read the public key */
  rc = gnutls_pubkey_export (crt_pk, GNUTLS_X509_FMT_DER, NULL, &size);

  if (rc == GNUTLS_E_SUCCESS || rc == GNUTLS_E_SHORT_MEMORY_BUFFER)
    {
      pubkey = g_byte_array_sized_new (size);
      pubkey->len = size;
      rc = gnutls_pubkey_export (crt_pk,
                                 GNUTLS_X509_FMT_DER,
                                 pubkey->data, &size);

      /* Intern the DER as private data */
      if (rc == GNUTLS_E_SUCCESS)
        {
          g_object_set_data_full (G_OBJECT (certificate),
                                  "valent-certificate-pk",
                                  g_steal_pointer (&pubkey),
                                  (GDestroyNotify)g_byte_array_unref);
        }
      else
        g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));
    }
  else
    g_warning ("%s(): %s", G_STRFUNC, gnutls_strerror (rc));

  out:
    gnutls_x509_crt_deinit (crt);
    gnutls_pubkey_deinit (crt_pk);

  return g_object_get_data (G_OBJECT (certificate), "valent-certificate-pk");
}

