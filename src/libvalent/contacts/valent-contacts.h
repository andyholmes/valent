// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_INSIDE) && !defined (VALENT_COMPILATION)
# error "Only <valent.h> can be included directly."
#endif

#include "../core/valent-component.h"
#include "valent-contact-store.h"


G_BEGIN_DECLS

#define VALENT_TYPE_CONTACTS (valent_contacts_get_type ())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentContacts, valent_contacts, VALENT, CONTACTS, ValentComponent)

VALENT_AVAILABLE_IN_1_0
ValentContacts     * valent_contacts_get_default  (void);

VALENT_AVAILABLE_IN_1_0
ValentContactStore * valent_contacts_ensure_store (ValentContacts *contacts,
                                                   const char     *uid,
                                                   const char     *name);

G_END_DECLS

