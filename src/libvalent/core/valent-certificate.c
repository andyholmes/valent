// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-utils"

#include "config.h"

#include <gio/gio.h>
#include <gnutls/gnutls.h>
#include <gnutls/abstract.h>
#include <gnutls/x509.h>
#include <sys/time.h>

#include "valent-certificate.h"

#define DEFAULT_EXPIRATION (60L*60L*24L*10L*365L)
#define DEFAULT_KEY_SIZE   4096

#define SHA256_HEX_LEN 64
#define SHA256_STR_LEN 96


/**
 * SECTION:valent-certificate
 * @short_description: Utilities for working with TLS Certificates
 * @title: Certificate Utilities
 * @stability: Unstable
 * @include: libvalent-core.h
 *
 * A small collection of helpers for working with TLS certificates.
 */

G_DEFINE_QUARK (VALENT_CERTIFICATE_FP, valent_certificate_fp);
G_DEFINE_QUARK (VALENT_CERTIFICATE_ID, valent_certificate_id);
G_DEFINE_QUARK (VALENT_CERTIFICATE_PK, valent_certificate_pk);


/**
 * valent_certificate_generate:
 * @key_path: file path to the private key
 * @cert_path: file path to the certificate
 * @common_name: common name for the certificate
 * @error: (nullable): a #GError
 *
 * Generate a private key and certificate for @common_name, saving them at
 * @key_path and @cert_path respectively.
 *
 * Returns: %TRUE if successful
 */
gboolean
valent_certificate_generate (const char  *key_path,
                             const char  *cert_path,
                             const char  *common_name,
                             GError     **error)
{
  int rc, ret;
  g_autofree char *dn = NULL;
  gnutls_x509_privkey_t privkey;
  gnutls_x509_crt_t crt;
  time_t timestamp;
  guint serial;
  gnutls_datum_t out;

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
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
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
    {
      gnutls_x509_privkey_deinit (privkey);
      return FALSE;
    }

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
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  /* Expiry (10 years) */
  timestamp = time (NULL);
  gnutls_x509_crt_set_activation_time (crt, timestamp);
  gnutls_x509_crt_set_expiration_time (crt, timestamp + DEFAULT_EXPIRATION);

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
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  /* Sign and export the certificate */
  if ((rc = gnutls_x509_crt_sign (crt, crt, privkey)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_export2 (crt, GNUTLS_X509_FMT_PEM, &out)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Signing certificate: %s",
                   gnutls_strerror (rc));
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  /* Output the certificate PEM to file */
  ret = g_file_set_contents_full (cert_path,
                                  (const char *)out.data,
                                  out.size,
                                  G_FILE_SET_CONTENTS_DURABLE,
                                  0600,
                                  error);
  gnutls_free (out.data);

  if (!ret)
    {
      gnutls_x509_crt_deinit (crt);
      gnutls_x509_privkey_deinit (privkey);

      return FALSE;
    }

  gnutls_x509_crt_deinit (crt);
  gnutls_x509_privkey_deinit (privkey);

  return TRUE;
}

/**
 * valent_certificate_get_id:
 * @certificate: a #GTlsCertificate
 * @error: (nullable): a #GError
 *
 * Get the common name from @certificate, which by convention is the single
 * source of truth for a device's ID.
 *
 * Returns: (transfer none) (nullable): the certificate ID
 */
const char *
valent_certificate_get_id (GTlsCertificate  *certificate,
                           GError          **error)
{
  const char *device_id;
  int rc;
  g_autoptr (GByteArray) ba = NULL;
  gnutls_x509_crt_t crt;
  gnutls_datum_t crt_der;
  char buf[64] = { 0, };
  size_t buf_size = 64;

  g_return_val_if_fail (G_IS_TLS_CERTIFICATE (certificate), NULL);

  /* Check */
  device_id = g_object_get_qdata (G_OBJECT (certificate),
                                  valent_certificate_id_quark());

  if G_LIKELY (device_id != NULL)
    return device_id;

  /* Extract the common name */
  g_object_get (certificate,
                "certificate", &ba,
                NULL);
  crt_der.data = ba->data;
  crt_der.size = ba->len;

  gnutls_x509_crt_init (&crt);

  if ((rc = gnutls_x509_crt_import (crt, &crt_der, 0)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_get_dn_by_oid (crt,
                                           GNUTLS_OID_X520_COMMON_NAME,
                                           0,
                                           0,
                                           &buf,
                                           &buf_size)) != GNUTLS_E_SUCCESS)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Reading common name: %s",
                   gnutls_strerror (rc));
      gnutls_x509_crt_deinit (crt);
      return NULL;
    }

  gnutls_x509_crt_deinit (crt);

  /* Intern the id as private data */
  g_object_set_qdata_full (G_OBJECT (certificate),
                           valent_certificate_id_quark(),
                           g_strndup (buf, buf_size),
                           g_free);

  return g_object_get_qdata (G_OBJECT (certificate),
                             valent_certificate_id_quark());
}

/**
 * valent_certificate_get_fingerprint:
 * @certificate: a #GTlsCertificate
 *
 * Get a SHA256 fingerprint hash of @certificate.
 *
 * Returns: (transfer none): a SHA256 hash
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

  fingerprint = g_object_get_qdata (G_OBJECT (certificate),
                                    valent_certificate_fp_quark());

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
  g_object_set_qdata_full (G_OBJECT (certificate),
                           valent_certificate_fp_quark(),
                           g_strdup (buf),
                           g_free);

  return g_object_get_qdata (G_OBJECT (certificate),
                             valent_certificate_fp_quark());
}

/**
 * valent_certificate_get_public_key:
 * @certificate: a #GTlsCertificate
 *
 * Get the public key of @certificate.
 *
 * Returns: (transfer none): a DER-encoded publickey
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

  pubkey = g_object_get_qdata (G_OBJECT (certificate),
                               valent_certificate_pk_quark());

  if (pubkey != NULL)
    return g_steal_pointer (&pubkey);

  g_object_get (certificate, "certificate", &certificate_der, NULL);
  crt_der.data = certificate_der->data;
  crt_der.size = certificate_der->len;

  /* Load the certificate */
  if ((rc = gnutls_x509_crt_init (&crt)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_x509_crt_import (crt, &crt_der, GNUTLS_X509_FMT_DER)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s: %s", G_STRFUNC, gnutls_strerror (rc));
      goto out;
    }

  /* Load the public key */
  if ((rc = gnutls_pubkey_init (&crt_pk)) != GNUTLS_E_SUCCESS ||
      (rc = gnutls_pubkey_import_x509 (crt_pk, crt, 0)) != GNUTLS_E_SUCCESS)
    {
      g_warning ("%s: %s", G_STRFUNC, gnutls_strerror (rc));
      goto out;
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

      /* Intern the PEM as private data */
      if (rc == GNUTLS_E_SUCCESS)
        {
          g_object_set_qdata_full (G_OBJECT (certificate),
                                   valent_certificate_pk_quark(),
                                   g_steal_pointer (&pubkey),
                                   (GDestroyNotify)g_byte_array_unref);
        }
      else
        g_warning ("%s: %s", G_STRFUNC, gnutls_strerror (rc));
    }
  else
    g_warning ("%s: %s", G_STRFUNC, gnutls_strerror (rc));

  out:
    gnutls_x509_crt_deinit (crt);
    gnutls_pubkey_deinit (crt_pk);

  return g_object_get_qdata (G_OBJECT (certificate),
                             valent_certificate_pk_quark());
}

