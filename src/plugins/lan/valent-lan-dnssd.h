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

G_END_DECLS
