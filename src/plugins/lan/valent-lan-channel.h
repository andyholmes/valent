// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>

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


#define VALENT_TYPE_LAN_CHANNEL (valent_lan_channel_get_type())

G_DECLARE_FINAL_TYPE (ValentLanChannel, valent_lan_channel, VALENT, LAN_CHANNEL, ValentChannel)

GTlsCertificate * valent_lan_channel_ref_certificate      (ValentLanChannel *lan_channel);
GTlsCertificate * valent_lan_channel_ref_peer_certificate (ValentLanChannel *lan_channel);
char            * valent_lan_channel_dup_host             (ValentLanChannel *lan_channel);
guint16           valent_lan_channel_get_port             (ValentLanChannel *lan_channel);

G_END_DECLS

