// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_LAN_DNSSD (valent_lan_dnssd_get_type())

G_DECLARE_FINAL_TYPE (ValentLanDNSSD, valent_lan_dnssd, VALENT, LAN_DNSSD, ValentObject)

GListModel * valent_lan_dnssd_new    (JsonNode       *identity);
void         valent_lan_dnssd_attach (ValentLanDNSSD *self,
                                      GMainContext   *context);

/*
 * GSocketAddress Helpers
 */
#define _g_socket_address_get_dnssd_name(a)                   \
  g_object_get_data (G_OBJECT (a), "dnssd-service-name")

#define _g_socket_address_set_dnssd_name(a, n)                \
  g_object_set_data_full (G_OBJECT (a), "dnssd-service-name", \
                          g_strdup (n), g_free)

G_END_DECLS
