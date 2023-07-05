// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

/**
 * VALENT_DNSSD_SERVICE_NAME: (value "dnssd-service-name")
 *
 * Constant for the DNS-SD service name, stored as private data on
 * [class@Gio.SocketAddress] objects.
 */
#define VALENT_DNSSD_SERVICE_NAME "dnssd-service-name"

/**
 * VALENT_DNSSD_SERVICE_TYPE: (value "_kdeconnect._udp")
 *
 * The DNS-SD service type used by KDE Connect.
 */
#define VALENT_DNSSD_SERVICE_TYPE "_kdeconnect._udp"

/**
 * ValentDNSSDState:
 * @VALENT_DNSSD_STATE_UNKNOWN: the state is unknown
 * @VALENT_DNSSD_STATE_REGISTERING: the local service is being registered
 * @VALENT_DNSSD_STATE_REGISTERED: the local service is registered
 * @VALENT_DNSSD_STATE_SYNCING: the network services are being synced
 * @VALENT_DNSSD_STATE_SYNCED: the network services are synced
 * @VALENT_DNSSD_STATE_ERROR: there was a fatal error
 *
 * Flags for DNS-SD service state.
 */
typedef enum {
  VALENT_DNSSD_STATE_NONE,
  VALENT_DNSSD_STATE_REGISTERING = 1 << 0,
  VALENT_DNSSD_STATE_REGISTERED  = 1 << 1,
  VALENT_DNSSD_STATE_SYNCING     = 1 << 2,
  VALENT_DNSSD_STATE_SYNCED      = 1 << 3,
  VALENT_DNSSD_STATE_ERROR       = 1 << 4,
} ValentDNSSDState;


#define VALENT_TYPE_LAN_DNSSD (valent_lan_dnssd_get_type())

G_DECLARE_FINAL_TYPE (ValentLanDNSSD, valent_lan_dnssd, VALENT, LAN_DNSSD, ValentObject)

GListModel * valent_lan_dnssd_new    (JsonNode        *identity);
void         valent_lan_dnssd_attach (ValentLanDNSSD  *self,
                                      GMainContext    *context);

G_END_DECLS
