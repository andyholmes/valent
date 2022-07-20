// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-lan-utils"

#include "config.h"

#include <gio/gio.h>
#include <gio/gnetworking.h>

#include "valent-lan-utils.h"


/**
 * configure_socket:
 * @connection: a #GSocketConnection
 *
 * Configure TCP socket options as they are set in kdeconnect-kde.
 *
 * Unlike kdeconnect-kde keepalive is not enabled if the required socket options
 * are not defined, otherwise connections may hang indefinitely.
 *
 * See: https://invent.kde.org/network/kdeconnect-kde/blob/master/core/backends/lan/lanlinkprovider.cpp
 */
static void
configure_socket (GSocketConnection *connection)
{
#if defined(TCP_KEEPIDLE) && defined(TCP_KEEPINTVL) && defined(TCP_KEEPCNT)
  GSocket *socket;
  GError *error = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));

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

static gboolean
certificate_from_device_id (const char       *device_id,
                            GTlsCertificate **certificate,
                            GError          **error)
{
  g_autoptr (GFile) file = NULL;

  g_assert (device_id != NULL);
  g_assert (certificate != NULL && *certificate == NULL);
  g_assert (error == NULL || *error == NULL);

  /* If no certificate exists we assume that's because the device is unpaired
   * and we're going to validate the certificate with user interaction */
  file = g_file_new_build_filename (g_get_user_config_dir(), PACKAGE_NAME,
                                    device_id, "certificate.pem",
                                    NULL);

  if (!g_file_query_exists (file, NULL))
    return TRUE;

  /* FIXME: handle the case of a corrupted certificate */
  *certificate = g_tls_certificate_new_from_file (g_file_peek_path (file),
                                                  error);

  if (*certificate == NULL)
    {
      //g_file_delete (cert_file, NULL, NULL);
      return FALSE;
    }

  return TRUE;
}

/*
 * The KDE Connect protocol follows a trust-on-first-use approach to TLS, so we
 * use a dummy callback for #GTlsConnection::accept-certificate that always
 * returns %TRUE.
 */
static gboolean
accept_certificate_cb (GTlsConnection       *connection,
                       GTlsCertificate      *peer_cert,
                       GTlsCertificateFlags  errors,
                       gpointer              user_data)
{
  return TRUE;
}

static gboolean
accept_certificate (GTlsConnection  *connection,
                    GCancellable    *cancellable,
                    GError         **error)
{
  unsigned long accept_id;
  gboolean ret;

  accept_id = g_signal_connect (G_OBJECT (connection),
                                "accept-certificate",
                                G_CALLBACK (accept_certificate_cb),
                                NULL);

  ret = g_tls_connection_handshake (connection, cancellable, error);
  g_clear_signal_handler (&accept_id, connection);

  return ret;
}

/**
 * handshake_id:
 * @conn: a #GTlsConnection
 * @device_id: the device id
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Wrap g_tls_connection_handshake() to implement KDE Connect's authentication.
 *
 * Lookup the TLS certificate for @device_id and compare it with the peer
 * certificate. If the device certificate is not available, the device is
 * assumed to be unpaired and %TRUE will be returned to trust-on-first-use which
 * allows pairing to happen later over an encrypted connection.
 *
 * Returns: %TRUE if the certificate matches or it is an unpaired device.
 */
static gboolean
handshake_id (GTlsConnection  *connection,
              const char      *device_id,
              GCancellable    *cancellable,
              GError         **error)
{
  g_autoptr (GTlsCertificate) trusted = NULL;
  GTlsCertificate *peer_cert;

  if (!accept_certificate (connection, cancellable, error))
    return FALSE;

  /* If the certificate existed but we failed to load it, we consider it an
   * authentication error.
   *
   * If there just was no certificate, its probably because we're unpaired and
   * we're trusting-on-first-use. */
  if (!certificate_from_device_id (device_id, &trusted, error))
    return FALSE;

  if (trusted == NULL)
    return TRUE;


  /* Compare the peer certificate with the cached certificate */
  peer_cert = g_tls_connection_get_peer_certificate (connection);

  if (!g_tls_certificate_is_same (trusted, peer_cert))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_HANDSHAKE,
                   "Invalid certificate for \"%s\"",
                   device_id);
      return FALSE;
    }

  return TRUE;
}

static gboolean
handshake_certificate (GTlsConnection   *connection,
                       GTlsCertificate  *trusted,
                       GCancellable     *cancellable,
                       GError          **error)
{
  GTlsCertificate *peer_cert;

  if (!accept_certificate (connection, cancellable, error))
    return FALSE;

  /* Compare the peer certificate with the supplied certificate */
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

/**
 * valent_lan_encrypt_new_client:
 * @connection: a #GSocketConnection
 * @device_id: the id for the device this connection claims to be from
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Set the standard KDE Connect socket options, wrap @connection in a
 * #GTlsClientConnection and authenticate it.
 *
 * This method is used for new connections when the certificate needs to be
 * pulled from the filesystem.
 *
 * Returns: (transfer full): a TLS encrypted #GIOStream
 */
GIOStream *
valent_lan_encrypt_new_client (GSocketConnection  *connection,
                               GTlsCertificate    *certificate,
                               const char         *device_id,
                               GCancellable       *cancellable,
                               GError            **error)
{
  g_autoptr (GSocketAddress) address = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  g_assert (device_id != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Set socket options */
  configure_socket (connection);

  /* Client encryption is used for incoming connections */
  address = g_socket_connection_get_remote_address(connection, error);

  if (address == NULL)
    return NULL;

  tls_stream = g_tls_client_connection_new (G_IO_STREAM (connection),
                                            G_SOCKET_CONNECTABLE (address),
                                            error);

  if (tls_stream == NULL)
    return NULL;

  /* Authorize the TLS connection */
  g_tls_connection_set_certificate (G_TLS_CONNECTION (tls_stream), certificate);

  if (!handshake_id (G_TLS_CONNECTION (tls_stream), device_id, cancellable, error))
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
 * @peer_cert: a #GTlsCertificate
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Set the standard KDE Connect socket options, wrap @connection in a
 * #GTlsClientConnection and authenticate it.
 *
 * This method is used for authenticating sub-connections (eg. transfers) when a
 * copy of the peer certificate is available to compare with.
 */
GIOStream *
valent_lan_encrypt_client (GSocketConnection  *connection,
                           GTlsCertificate    *certificate,
                           GTlsCertificate    *peer_cert,
                           GCancellable       *cancellable,
                           GError            **error)
{
  g_autoptr (GSocketAddress) address = NULL;
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  //g_assert (G_IS_TLS_CERTIFICATE (peer_cert));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* TODO: Occasionally we are not passed a certificate. This could mean the
   * parent connection is unauthorized, but more likely there is a logic error
   * elsewhere where we're making a false assumption. */
  if G_UNLIKELY (!G_IS_TLS_CERTIFICATE (peer_cert))
    {
      g_set_error (error,
                   G_TLS_ERROR,
                   G_TLS_ERROR_CERTIFICATE_REQUIRED,
                   "No peer certificate");
      return NULL;
    }

  /* Set socket options */
  configure_socket (connection);

  /* Client encryption is used for incoming connections */
  address = g_socket_connection_get_remote_address(connection, error);

  if (address == NULL)
    return NULL;

  tls_stream = g_tls_client_connection_new (G_IO_STREAM (connection),
                                            G_SOCKET_CONNECTABLE (address),
                                            error);

  if (tls_stream == NULL)
    return NULL;

  /* Authorize the TLS connection */
  g_tls_connection_set_certificate (G_TLS_CONNECTION (tls_stream), certificate);

  if (!handshake_certificate (G_TLS_CONNECTION (tls_stream),
                              peer_cert,
                              cancellable,
                              error))
    {
      g_io_stream_close (tls_stream, NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

/**
 * valent_lan_encrypt_new_server:
 * @connection: a #GSocketConnection
 * @device_id: the id for the device this connection claims to be from
 * @cancellable: (nullable): a #GCancellable
 * @error: (nullable): a #GError
 *
 * Set the standard KDE Connect socket options, wrap @connection in a
 * #GTlsServerConnection and authenticate it.
 *
 * This method is used for new connections when the certificate needs to be
 * pulled from the filesystem.
 *
 * Returns: (type Gio.IOStream) (transfer full): a TLS encrypted #GIOStream
 */
GIOStream *
valent_lan_encrypt_new_server (GSocketConnection  *connection,
                               GTlsCertificate    *certificate,
                               const char         *device_id,
                               GCancellable       *cancellable,
                               GError            **error)
{
  g_autoptr (GIOStream) tls_stream = NULL;

  g_assert (G_IS_SOCKET_CONNECTION (connection));
  g_assert (G_IS_TLS_CERTIFICATE (certificate));
  g_assert (device_id != NULL);
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_assert (error == NULL || *error == NULL);

  /* Set socket options */
  configure_socket (connection);

  /* Server encryption is used for responses to identity broadcasts */
  tls_stream = g_tls_server_connection_new (G_IO_STREAM (connection),
                                            certificate,
                                            error);

  if (tls_stream == NULL)
    return NULL;

  /* Set the connection certificate */
  g_object_set (G_TLS_SERVER_CONNECTION (tls_stream),
                "authentication-mode", G_TLS_AUTHENTICATION_REQUIRED,
                NULL);

  /* Authorize the TLS connection */
  if (!handshake_id (G_TLS_CONNECTION (tls_stream), device_id, cancellable, error))
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
 * Set the standard KDE Connect socket options, wrap @connection in a
 * #GTlsServerConnection and authenticate it.
 *
 * This method is used for authenticating sub-connections (eg. transfers) when a
 * copy of the peer certificate is available to compare with.
 *
 * Returns: (type Gio.IOStream) (transfer full): a TLS encrypted #GIOStream
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

  /* Set socket options */
  configure_socket (connection);

  /* Server encryption is used for responses to identity broadcasts */
  tls_stream = g_tls_server_connection_new (G_IO_STREAM (connection),
                                            certificate,
                                            error);

  if (tls_stream == NULL)
    return NULL;

  /* Set the connection certificate */
  g_object_set (G_TLS_SERVER_CONNECTION (tls_stream),
                "authentication-mode", G_TLS_AUTHENTICATION_REQUIRED,
                NULL);

  /* Authorize the TLS connection */
  if (!handshake_certificate (G_TLS_CONNECTION (tls_stream),
                              peer_certificate,
                              cancellable,
                              error))
    {
      g_io_stream_close (tls_stream, NULL, NULL);
      return NULL;
    }

  return g_steal_pointer (&tls_stream);
}

