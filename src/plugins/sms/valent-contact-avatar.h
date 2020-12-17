// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gtk/gtk.h>
#include <libvalent-contacts.h>

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_AVATAR (valent_contact_avatar_get_type())

G_DECLARE_FINAL_TYPE (ValentContactAvatar, valent_contact_avatar, VALENT, CONTACT_AVATAR, GtkWidget)

GtkWidget * valent_contact_avatar_new         (EContact            *contact);
GtkWidget * valent_contact_avatar_copy        (ValentContactAvatar *avatar);
EContact  * valent_contact_avatar_get_contact (ValentContactAvatar *avatar);
void        valent_contact_avatar_set_contact (ValentContactAvatar *avatar,
                                               EContact            *contact);

G_END_DECLS
