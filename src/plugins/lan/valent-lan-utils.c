// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-utils"

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <valent.h>

#include "valent-lan-utils.h"


/*
 * The KDE Connect protocol follows a trust-on-first-use approach to TLS, so we
 * use a dummy callback for `GTlsConnection`::accept-certificate that always
 * returns %TRUE.
 */
static gboolean
valent_lan_accept_certificate_cb (GTlsConnection       *connection,
                                  GTlsCertificate      *peer_cert,
                                  GTlsCertificateFlags  errors,
                                  gpointer              user_data)
{
  return TRUE;
}

static gboolean
valent_lan_accept_certificate (GTlsConnection  *connection,
                               GCancellable    *cancellable,
                               GError         **error)
{
  unsigned long accept_id;
  gboolean ret;

  accept_id = g_signal_connect (G_OBJECT (connection),
                                "accept-certificate",
                                G_CALLBACK (valent_lan_accept_certificate_cb),
                                NULL);

  ret = g_tls_connection_handshake (connection, cancellable, error);
  g_clear_signal_handler (&accept_id, connection);

  return ret;
}

/* < private >
 * valent_lan_handshake_certificate:
 * @connection: a `GTlsConnection`
 * @trusted: a `GTlsCertificate`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Authenticate a connection for a known peer.
 *
 * This function is used to authenticate a TLS connection against a known and
 * trusted TLS certificate. This should be used to authenticate auxiliary
 * connections for authenticated channels.
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
static gboolean
valent_lan_handshake_certificate (GTlsConnection   *connection,
                                  GTlsCertificate  *trusted,
                                  GCancellable     *cancellable,
                                  GError          **error)
{
  GTlsCertificate *peer_certificate;

  if (!valent_lan_accept_certificate (connection, cancellable, error))
    return FALSE;

  peer_certificate = g_tls_connection_get_peer_certificate (connection);
  if (!g_tls_certificate_is_same (trusted, peer_certificate))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_HANDSHAKE,
                   "Peer certificate does not match trusted certificate for \"%s\"",
                   valent_certificate_get_common_name (peer_certificate));
      return FALSE;
    }

  return TRUE;
}

/* < private >
 * valent_lan_handshake_peer:
 * @connection: a `GTlsConnection`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Authenticate a connection for an unknown peer.
 *
 * This function is used to authenticate a TLS connection whether the remote
 * device is paired or not. This should be used to authenticate new connections
 * when negotiating a [class@Valent.LanChannel].
 *
 * If the TLS certificate is not known (i.e. previously authenticated), the
 * device is assumed to be unpaired and %TRUE will be returned to
 * trust-on-first-use. The certificate will become "known" when if and when the
 * device is successfully paired.
 *
 * Returns: %TRUE, or %FALSE with @error set
 */
static gboolean
valent_lan_handshake_peer (GTlsConnection  *connection,
                           GCancellable    *cancellable,
                           GError         **error)
{
  GTlsCertificate *peer_certificate;
  const char *peer_id;
  g_autofree char *trusted_path = NULL;
  g_autoptr (GTlsCertificate) trusted_cert = NULL;
  g_autoptr (GError) cert_error = NULL;

  if (!valent_lan_accept_certificate (connection, cancellable, error))
    return FALSE;

  peer_certificate = g_tls_connection_get_peer_certificate (connection);
  peer_id = valent_certificate_get_common_name (peer_certificate);

  // TODO: this should be handled by centralized manager object
  trusted_path = g_build_filename (g_get_user_config_dir(), PACKAGE_NAME,
                                   "device", peer_id,
                                   "certificate.pem",
                                   NULL);
  trusted_cert = g_tls_certificate_new_from_file (trusted_path, &cert_error);
  if (trusted_cert == NULL)
    {
      if (cert_error->domain != G_FILE_ERROR)
        {
          g_propagate_error (error, g_steal_pointer (&cert_error));
          return FALSE;
        }

      VALENT_NOTE ("Accepting certificate from \"%s\" on a trust-on-first-use basis",
                   peer_id);
      return TRUE;
    }

  if (!g_tls_certificate_is_same (trusted_cert, peer_certificate))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_HANDSHAKE,
                   "Peer certificate does not match trusted certificate for \"%s\"",
                   peer_id);
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_lan_encrypt_client_connection:
 * @connection: a `GSocketConnection`
 * @certificate: a `GTlsCertificate`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Authenticate and encrypt a client connection.
 *
 * This function sets the standard KDE Connect socket options on @connection,
 * wraps it in a [class@Gio.TlsConnection] and returns the result.
 *
 * The common name is extracted from the peer's TLS certificate and used as the
 * device ID to check for a trusted certificate. For auxiliary connections
 * created from an existing channel, use [func@Valent.lan_encrypt_client].
 *
 * Returns: (transfer full) (nullable): a TLS encrypted `GIOStream`
 */
GIOStream *
valent_lan_encrypt_client_connection (GSocketConnection  *connection,
                                      GTlsCertificate    *certificate,
                                      GCancellable       *cancellable,
                                      GError            **error)
{
  g_autoptr (GSocketAddress) address = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* We're the client when accepting incoming connections
   */
  g_socket_set_keepalive (g_socket_connection_get_socket (connection), TRUE);
  address = g_socket_connection_get_remote_address (connection, error);
  if (address == NULL)
    return NULL;

  tls_stream = g_tls_client_connection_new (G_IO_STREAM (connection),
                                            G_SOCKET_CONNECTABLE (address),
                                            error);

  if (tls_stream == NULL)
    return NULL;

  g_tls_connection_set_certificate (G_TLS_CONNECTION (tls_stream), certificate);

  if (!valent_lan_handshake_peer (G_TLS_CONNECTION (tls_stream),
                                  cancellable,
                                  error))
    {
      g_io_stream_close (tls_stream, NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

/**
 * valent_lan_encrypt_client:
 * @connection: a `GSocketConnection`
 * @certificate: a `GTlsCertificate`
 * @peer_certificate: a `GTlsCertificate`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Authenticate and encrypt an auxiliary client connection.
 *
 * This function sets the standard KDE Connect socket options on @connection,
 * wraps it in a [class@Gio.TlsConnection] and returns the result.
 *
 * Returns: (transfer full) (nullable): a TLS encrypted `GIOStream`
 */
GIOStream *
valent_lan_encrypt_client (GSocketConnection  *connection,
                           GTlsCertificate    *certificate,
                           GTlsCertificate    *peer_certificate,
                           GCancellable       *cancellable,
                           GError            **error)
{
  g_autoptr (GSocketAddress) address = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  //g_assert (G_IS_TLS_CERTIFICATE (peer_certificate));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  // TODO: Occasionally we are not passed a certificate
  if G_UNLIKELY (!G_IS_TLS_CERTIFICATE (peer_certificate))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_CERTIFICATE_REQUIRED,
                   "No peer certificate");
      return NULL;
    }

  /* We're the client when accepting auxiliary connections
   */
  g_socket_set_keepalive (g_socket_connection_get_socket (connection), TRUE);
  address = g_socket_connection_get_remote_address (connection, error);
  if (address == NULL)
    return NULL;

  tls_stream = g_tls_client_connection_new (G_IO_STREAM (connection),
                                            G_SOCKET_CONNECTABLE (address),
                                            error);

  if (tls_stream == NULL)
    return NULL;

  g_tls_connection_set_certificate (G_TLS_CONNECTION (tls_stream), certificate);

  if (!valent_lan_handshake_certificate (G_TLS_CONNECTION (tls_stream),
                                         peer_certificate,
                                         cancellable,
                                         error))
    {
      g_io_stream_close (tls_stream, NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

/**
 * valent_lan_encrypt_server_connection:
 * @connection: a `GSocketConnection`
 * @certificate: a `GTlsConnection`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Authenticate and encrypt a server connection.
 *
 * This function sets the standard KDE Connect socket options on @connection,
 * wraps it in a [class@Gio.TlsConnection] and returns the result.
 *
 * The common name is extracted from the peer's TLS certificate and used as the
 * device ID to check for a trusted certificate. For auxiliary connections
 * created from an existing channel, use [func@Valent.lan_encrypt_server].
 *
 * Returns: (transfer full) (nullable): a TLS encrypted `GIOStream`
 */
GIOStream *
valent_lan_encrypt_server_connection (GSocketConnection  *connection,
                                      GTlsCertificate    *certificate,
                                      GCancellable       *cancellable,
                                      GError            **error)
{
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* We're the server when opening outgoing connections
   */
  g_socket_set_keepalive (g_socket_connection_get_socket (connection), TRUE);
  tls_stream = g_tls_server_connection_new (G_IO_STREAM (connection),
                                            certificate,
                                            error);

  if (tls_stream == NULL)
    return NULL;

  g_object_set (G_TLS_SERVER_CONNECTION (tls_stream),
                "authentication-mode", G_TLS_AUTHENTICATION_REQUIRED,
                NULL);

  if (!valent_lan_handshake_peer (G_TLS_CONNECTION (tls_stream),
                                  cancellable,
                                  error))
    {
      g_io_stream_close (tls_stream, NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

/**
 * valent_lan_encrypt_server:
 * @connection: a `GSocketConnection`
 * @certificate: a `GTlsCertificate`
 * @peer_certificate: a `GTlsCertificate`
 * @cancellable: (nullable): a `GCancellable`
 * @error: (nullable): a `GError`
 *
 * Authenticate and encrypt an auxiliary server connection.
 *
 * This function sets the standard KDE Connect socket options on @connection,
 * wraps it in a [class@Gio.TlsConnection] and returns the result.
 *
 * Returns: (transfer full) (nullable): a TLS encrypted `GIOStream`
 */
GIOStream *
valent_lan_encrypt_server (GSocketConnection  *connection,
                           GTlsCertificate    *certificate,
                           GTlsCertificate    *peer_certificate,
                           GCancellable       *cancellable,
                           GError            **error)
{
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  g_assert (G_IS_TLS_CERTIFICATE (peer_certificate));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* We're the server when accepting auxiliary connections
   */
  g_socket_set_keepalive (g_socket_connection_get_socket (connection), TRUE);
  tls_stream = g_tls_server_connection_new (G_IO_STREAM (connection),
                                            certificate,
                                            error);

  if (tls_stream == NULL)
    return NULL;

  g_object_set (G_TLS_SERVER_CONNECTION (tls_stream),
                "authentication-mode", G_TLS_AUTHENTICATION_REQUIRED,
                NULL);

  if (!valent_lan_handshake_certificate (G_TLS_CONNECTION (tls_stream),
                                         peer_certificate,
                                         cancellable,
                                         error))
    {
      g_io_stream_close (tls_stream, NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

