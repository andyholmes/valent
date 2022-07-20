// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_TYPE_LAN_CHANNEL (valent_lan_channel_get_type())

G_DECLARE_FINAL_TYPE (ValentLanChannel, valent_lan_channel, VALENT, LAN_CHANNEL, ValentChannel)

/* Properties */
GTlsCertificate * valent_lan_channel_ref_certificate      (ValentLanChannel *lan_channel);
GTlsCertificate * valent_lan_channel_ref_peer_certificate (ValentLanChannel *lan_channel);
char            * valent_lan_channel_dup_host             (ValentLanChannel *lan_channel);
guint16           valent_lan_channel_get_port             (ValentLanChannel *lan_channel);

G_END_DECLS

