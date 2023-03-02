// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gdk/gdk.h>
#include <valent.h>

G_BEGIN_DECLS

void       valent_sms_avatar_from_contact       (AdwAvatar            *avatar,
                                                 EContact             *contact);
void       valent_sms_contact_from_phone        (ValentContactStore   *store,
                                                 const char           *number,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
EContact * valent_sms_contact_from_phone_finish (ValentContactStore   *store,
                                                 GAsyncResult         *result,
                                                 GError              **error);

gboolean   valent_phone_number_equal            (const char            *number1,
                                                 const char            *number2);
char     * valent_phone_number_normalize        (const char            *number);
gboolean   valent_phone_number_of_contact       (EContact              *contact,
                                                 const char            *number);

G_END_DECLS
