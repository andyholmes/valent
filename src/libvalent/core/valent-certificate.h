// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

gboolean     valent_certificate_generate        (const char       *key_path,
                                                 const char       *cert_path,
                                                 const char       *common_name,
                                                 GError          **error);
const char * valent_certificate_get_id          (GTlsCertificate  *certificate,
                                                 GError          **error);
const char * valent_certificate_get_fingerprint (GTlsCertificate  *certificate);
GByteArray * valent_certificate_get_public_key  (GTlsCertificate  *certificate);

G_END_DECLS

