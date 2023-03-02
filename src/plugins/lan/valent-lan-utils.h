// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

/**
 * VALENT_LAN_PROTOCOL_ADDR: (value "255.255.255.255")
 *
 * The default broadcast address used by the KDE Connect protocol.
 */
#define VALENT_LAN_PROTOCOL_ADDR     "255.255.255.255"

/**
 * VALENT_LAN_PROTOCOL_PORT: (value 1716)
 *
 * The default port used by the KDE Connect protocol for UDP discovery and
 * JSON packet exchange.
 */
#define VALENT_LAN_PROTOCOL_PORT     (1716)

/**
 * VALENT_LAN_PROTOCOL_PORT_MIN: (value 1716)
 *
 * The minimum port used by the KDE Connect protocol.
 */
#define VALENT_LAN_PROTOCOL_PORT_MIN (1716)

/**
 * VALENT_LAN_PROTOCOL_PORT_MAX: (value 1764)
 *
 * The maximum port used by the KDE Connect protocol.
 */
#define VALENT_LAN_PROTOCOL_PORT_MAX (1764)

/**
 * VALENT_LAN_TRANSFER_PORT_MIN: (value 1739)
 *
 * The minimum port used by the KDE Connect protocol for auxiliary streams,
 * such as file transfers.
 */
#define VALENT_LAN_TRANSFER_PORT_MIN (1739)

/**
 * VALENT_LAN_PROTOCOL_PORT_MIN: (value 1764)
 *
 * The maximum port used by the KDE Connect protocol for auxiliary streams,
 * such as file transfers.
 */
#define VALENT_LAN_TRANSFER_PORT_MAX (1764)


GIOStream * valent_lan_encrypt_client            (GSocketConnection  *connection,
                                                  GTlsCertificate    *certificate,
                                                  GTlsCertificate    *peer_cert,
                                                  GCancellable       *cancellable,
                                                  GError            **error);
GIOStream * valent_lan_encrypt_client_connection (GSocketConnection  *connection,
                                                  GTlsCertificate    *certificate,
                                                  GCancellable       *cancellable,
                                                  GError            **error);
GIOStream * valent_lan_encrypt_server            (GSocketConnection  *connection,
                                                  GTlsCertificate    *certificate,
                                                  GTlsCertificate    *peer_cert,
                                                  GCancellable       *cancellable,
                                                  GError            **error);
GIOStream * valent_lan_encrypt_server_connection (GSocketConnection  *connection,
                                                  GTlsCertificate    *certificate,
                                                  GCancellable       *cancellable,
                                                  GError            **error);

G_END_DECLS

