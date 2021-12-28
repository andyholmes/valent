// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CORE_INSIDE) && !defined (VALENT_CORE_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <gio/gio.h>

G_BEGIN_DECLS

void              valent_certificate_new             (const char           *path,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
GTlsCertificate * valent_certificate_new_finish      (GAsyncResult         *result,
                                                      GError              **error);
GTlsCertificate * valent_certificate_new_sync        (const char           *path,
                                                      GError              **error);
const char      * valent_certificate_get_common_name (GTlsCertificate  *certificate);
const char      * valent_certificate_get_fingerprint (GTlsCertificate  *certificate);
GByteArray      * valent_certificate_get_public_key  (GTlsCertificate  *certificate);

G_END_DECLS

