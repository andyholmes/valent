// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

GIOStream * valent_lan_encrypt_new_client      (GSocketConnection  *connection,
                                                GTlsCertificate    *certificate,
                                                const char         *device_id,
                                                GCancellable       *cancellable,
                                                GError            **error);
GIOStream * valent_lan_encrypt_client          (GSocketConnection  *connection,
                                                GTlsCertificate    *certificate,
                                                GTlsCertificate    *peer_cert,
                                                GCancellable       *cancellable,
                                                GError            **error);
GIOStream * valent_lan_encrypt_new_server      (GSocketConnection  *connection,
                                                GTlsCertificate    *certificate,
                                                const char         *device_id,
                                                GCancellable       *cancellable,
                                                GError            **error);
GIOStream * valent_lan_encrypt_server          (GSocketConnection  *connection,
                                                GTlsCertificate    *certificate,
                                                GTlsCertificate    *peer_cert,
                                                GCancellable       *cancellable,
                                                GError            **error);

G_END_DECLS

