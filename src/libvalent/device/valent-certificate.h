// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-version.h"

G_BEGIN_DECLS

VALENT_AVAILABLE_IN_1_0
void              valent_certificate_new             (const char           *path,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
VALENT_AVAILABLE_IN_1_0
GTlsCertificate * valent_certificate_new_finish      (GAsyncResult         *result,
                                                      GError              **error);
VALENT_AVAILABLE_IN_1_0
GTlsCertificate * valent_certificate_new_sync        (const char           *path,
                                                      GError              **error);
VALENT_AVAILABLE_IN_1_0
const char      * valent_certificate_get_common_name (GTlsCertificate  *certificate);
VALENT_AVAILABLE_IN_1_0
const char      * valent_certificate_get_fingerprint (GTlsCertificate  *certificate);
VALENT_AVAILABLE_IN_1_0
GByteArray      * valent_certificate_get_public_key  (GTlsCertificate  *certificate);

G_END_DECLS

