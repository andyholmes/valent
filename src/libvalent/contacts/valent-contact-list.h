// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "../core/valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_LIST (valent_contact_list_get_type())

G_DECLARE_FINAL_TYPE (ValentContactList, valent_contact_list, VALENT, CONTACT_LIST, ValentObject)

G_END_DECLS
