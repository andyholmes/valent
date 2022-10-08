// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-utils"

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>
#include <libvalent-core.h>

#include "valent-lan-utils.h"


/* < private >
 * valent_lan_configure_socket:
 * @connection: a #GSocketConnection
 *
 * Configure TCP socket options as they are set in kdeconnect-kde.
 *
 * Unlike kdeconnect-kde keepalive is not enabled if the required socket options
 * are not defined, otherwise connections may hang indefinitely.
 *
 * See: https://invent.kde.org/network/kdeconnect-kde/blob/master/core/backends/lan/lanlinkprovider.cpp
 */
static inline void
valent_lan_configure_socket (GSocketConnection *connection)
{
#if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL) && defined(TCP_KEEPCNT)
  GSocket *socket;
  GError *error = NULL;

  g_assert (G_IS_TCP_CONNECTION (connection));

  socket = g_socket_connection_get_socket (connection);
  g_socket_set_keepalive (socket, TRUE);

  if (!g_socket_set_option (socket, IPPROTO_TCP, TCP_KEEPIDLE, 10, &error))
    {
      g_warning ("%s(): TCP_KEEPIDLE: %s", G_STRFUNC, error->message);
      g_clear_error (&error);
    }

  if (!g_socket_set_option (socket, IPPROTO_TCP, TCP_KEEPINTVL, 5, &error))
    {
      g_warning ("%s(): TCP_KEEPINTVL: %s", G_STRFUNC, error->message);
      g_clear_error (&error);
    }

  if (!g_socket_set_option (socket, IPPROTO_TCP, TCP_KEEPCNT, 3, &error))
    {
      g_warning ("%s(): TCP_KEEPCNT: %s", G_STRFUNC, error->message);
      g_clear_error (&error);
    }
#endif /* TCP_KEEPIDLE && TCP_KEEPINTVL && TCP_KEEPCNT */
}


/*
 * The KDE Connect protocol follows a trust-on-first-use approach to TLS, so we
 * use a dummy callback for #GTlsConnection::accept-certificate that always
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
 * @connection: a #GTlsConnection
 * @trusted: a #GTlsCertificate
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
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
  GTlsCertificate *peer_cert;

  if (!valent_lan_accept_certificate (connection, cancellable, error))
    return FALSE;

  peer_cert = g_tls_connection_get_peer_certificate (connection);

  if (!g_tls_certificate_is_same (trusted, peer_cert))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_HANDSHAKE,
                   "Invalid certificate");
      return FALSE;
    }

  return TRUE;
}

/* < private >
 * valent_lan_handshake_peer:
 * @connection: a #GTlsConnection
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
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
  g_autoptr (GFile) file = NULL;
  g_autoptr (GTlsCertificate) peer_trusted = NULL;
  GTlsCertificate *peer_certificate;
  const char *peer_id;

  if (!valent_lan_accept_certificate (connection, cancellable, error))
    return FALSE;

  peer_certificate = g_tls_connection_get_peer_certificate (connection);
  peer_id = valent_certificate_get_common_name (peer_certificate);

  /* If the certificate can not be found, assume that's because the device is
   * unpaired and the certificate will be verified with user interaction */
  file = g_file_new_build_filename (g_get_user_config_dir(), PACKAGE_NAME,
                                    peer_id, "certificate.pem",
                                    NULL);

  if (!g_file_query_exists (file, NULL))
    return TRUE;

  peer_trusted = g_tls_certificate_new_from_file (g_file_peek_path (file),
                                                  error);

  // TODO: handle the case of a corrupted certificate
  if (peer_trusted == NULL)
    return FALSE;

  if (!g_tls_certificate_is_same (peer_trusted, peer_certificate))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_HANDSHAKE,
                   "Invalid certificate for \"%s\"",
                   peer_id);
      return FALSE;
    }

  return TRUE;
}

/**
 * valent_lan_encrypt_client_connection:
 * @connection: a #GSocketConnection
 * @certificate: a #GTlsCertificate
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
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
 * Returns: (transfer full) (nullable): a TLS encrypted #GIOStream
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

  valent_lan_configure_socket (connection);

  /* We're the client when accepting incoming connections */
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
 * @connection: a #GSocketConnection
 * @certificate: a #GTlsCertificate
 * @peer_certificate: a #GTlsCertificate
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Authenticate and encrypt an auxiliary client connection.
 *
 * This function sets the standard KDE Connect socket options on @connection,
 * wraps it in a [class@Gio.TlsConnection] and returns the result.
 *
 * Returns: (transfer full) (nullable): a TLS encrypted #GIOStream
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

  /* TODO: Occasionally we are not passed a certificate. This could mean the
   * parent connection is unauthorized, but more likely there is a logic error
   * elsewhere where we're making a false assumption. */
  if G_UNLIKELY (!G_IS_TLS_CERTIFICATE (peer_certificate))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_CERTIFICATE_REQUIRED,
                   "No peer certificate");
      return NULL;
    }

  valent_lan_configure_socket (connection);

  address = g_socket_connection_get_remote_address (connection, error);

  if (address == NULL)
    return NULL;

  /* We're the client when opening auxiliary connections */
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
 * @connection: a #GSocketConnection
 * @certificate: a #GTlsConnection
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
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
 * Returns: (transfer full) (nullable): a TLS encrypted #GIOStream
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

  valent_lan_configure_socket (connection);

  /* We're the server when opening outgoing connections */
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
 * @connection: a #GSocketConnection
 * @certificate: a #GTlsCertificate
 * @peer_certificate: a #GTlsCertificate
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Authenticate and encrypt an auxiliary server connection.
 *
 * This function sets the standard KDE Connect socket options on @connection,
 * wraps it in a [class@Gio.TlsConnection] and returns the result.
 *
 * Returns: (transfer full) (nullable): a TLS encrypted #GIOStream
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

  valent_lan_configure_socket (connection);

  /* We're the server when accepting auxiliary connections */
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

