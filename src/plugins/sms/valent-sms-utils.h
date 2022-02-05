// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gdk/gdk.h>
#include <libvalent-contacts.h>

G_BEGIN_DECLS

void   valent_sms_avatar_from_contact (AdwAvatar *avatar,
                                       EContact  *contact);

G_END_DECLS
