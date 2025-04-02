// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-utils"

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <valent.h>

#include "valent-lan-utils.h"


/* KDE Connect uses self-signed certificates with a trust-on-first-use policy,
 * and since GLib can not guarantee all certificate errors are set, the
 * `accept-certificate` callback always returns %TRUE.
 */
static gboolean
valent_lan_connection_accept_certificate_cb (GTlsConnection       *connection,
                                             GTlsCertificate      *peer_cert,
                                             GTlsCertificateFlags  errors,
                                             gpointer              user_data)
{
  return TRUE;
}

/**
 * valent_lan_connection_handshake:
 * @connection: a `GSocketConnection`
 * @certificate: a `GTlsCertificate`
 * @trusted: (nullable): a `GTlsCertificate`
 * @is_client: %TRUE for client connection, or %FALSE for a server connection
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Wrap @connection in a [class@Gio.TlsConnection] and perform the handshake.
 *
 * If @trusted is not %NULL, the remote device will be expected to handshake
 * use the same certificate, otherwise trusted devices will be searched for
 * a certificate. If the device is unknown, the certificate will be verified
 * as self-signed and accepted on a trust-on-first-use basis.
 *
 * If @is_client is %TRUE, this will create a [class@Gio.TlsClientConnection],
 * otherwise a [class@Gio.TlsServerConnection].
 *
 * Returns: (transfer full) (nullable): a TLS encrypted `GIOStream`
 */
GIOStream *
valent_lan_connection_handshake (GSocketConnection  *connection,
                                 GTlsCertificate    *certificate,
                                 GTlsCertificate    *trusted,
                                 gboolean            is_client,
                                 GCancellable       *cancellable,
                                 GError            **error)
{
  GTlsBackend *backend;
  g_autoptr (GIOStream) ret = NULL;
  g_autoptr (GTlsCertificate) ca_certificate = NULL;
  GTlsCertificate *peer_certificate;
  const char *peer_common_name = NULL;
  GTlsCertificateFlags errors = 0;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  g_assert (trusted == NULL || G_IS_TLS_CERTIFICATE (trusted));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  g_socket_set_keepalive (g_socket_connection_get_socket (connection), TRUE);

  /* NOTE: When negotiating the primary connection, a KDE Connect device
   *       acts as the TLS server when opening TCP connections,
   *       and the TLS client when accepting TCP connections.
   *
   *       When negotiating an auxiliary connection, a KDE Connect device
   *       acts as the TLS server when accepting TCP connections,
   *       and the TLS client when opening TCP connections.
   *
   * TODO: If a database returned by [ctor@Gio.TlsFileDatabase.new] were used
   *       for trusted certificates, this function could return immediately
   *       after a handshake with no errors.
   *
   * TODO: KDE Connect uses TLS/SNI to communicate the expected device ID, but
   *       but GnuTLS refuses server names that are not valid hostnames.
   */
  backend = g_tls_backend_get_default ();
  if (is_client)
    {
      ret = g_initable_new (g_tls_backend_get_client_connection_type (backend),
                            cancellable,
                            error,
                            "base-io-stream",  connection,
                            "certificate",     certificate,
                            // TODO: see note above about TLS/SNI
                            "server-identity", NULL,
                            NULL);
    }
  else
    {
      ret = g_initable_new (g_tls_backend_get_server_connection_type (backend),
                            cancellable,
                            error,
                            "authentication-mode", G_TLS_AUTHENTICATION_REQUIRED,
                            "base-io-stream",      connection,
                            "certificate",         certificate,
                            NULL);
    }

  if (ret == NULL)
    return NULL;

  /* Perform TLS handshake to get the peer certificate, which should only
   * fail on cancellation.
   */
  g_signal_connect (G_TLS_CONNECTION (ret),
                    "accept-certificate",
                    G_CALLBACK (valent_lan_connection_accept_certificate_cb),
                    NULL);

  if (!g_tls_connection_handshake (G_TLS_CONNECTION (ret), cancellable, error))
    return NULL;

  /* The certificate common name is validated as a device ID after the
   * handshake, to sanitize queries to the filesystem for a trusted certificate.
   */
  peer_certificate = g_tls_connection_get_peer_certificate (G_TLS_CONNECTION (ret));
  peer_common_name = valent_certificate_get_common_name (peer_certificate);
  if (!valent_device_validate_id (peer_common_name))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_HANDSHAKE,
                   "Certificate common name is not a valid device ID \"%s\"",
                   peer_common_name);
      return NULL;
    }

  /* If @trusted is not provided, try to find a certificate in a device
   * config directory. If neither are found, the certificate is verified
   * as self-signed before being accepted per the trust-on-first-use policy.
   *
   * TODO: this should be handled by a managed cache object
   */
  if (trusted != NULL)
    {
      ca_certificate = g_object_ref (trusted);
    }
  else
    {
      g_autofree char *trusted_path = NULL;

      trusted_path = g_build_filename (g_get_user_config_dir(), PACKAGE_NAME,
                                       "device", peer_common_name,
                                       "certificate.pem",
                                       NULL);
      ca_certificate = g_tls_certificate_new_from_file (trusted_path, NULL);
      if (ca_certificate == NULL)
        ca_certificate = g_object_ref (peer_certificate);
    }

  errors = g_tls_certificate_verify (peer_certificate, NULL, ca_certificate);
  if (errors != G_TLS_CERTIFICATE_NO_FLAGS)
    {
      g_autofree char *errors_str = NULL;

      errors_str = g_flags_to_string (G_TYPE_TLS_CERTIFICATE_FLAGS, errors);
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_HANDSHAKE,
                   "Certificate for \"%s\" failed verification (%s)",
                   peer_common_name,
                   errors_str);
      return NULL;
    }

  g_debug (ca_certificate == peer_certificate
             ? "Accepting certificate \"%s\" per trust-on-first-use policy"
             : "Accepting certificate \"%s\" per successful verification",
           peer_common_name);

  return g_steal_pointer (&ret);
}

