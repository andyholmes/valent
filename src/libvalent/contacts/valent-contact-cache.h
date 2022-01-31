// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_CONTACTS_INSIDE) && !defined (VALENT_CONTACTS_COMPILATION)
# error "Only <libvalent-contacts.h> can be included directly."
#endif

#include <libvalent-core.h>

#include "valent-contact-store.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_CACHE (valent_contact_cache_get_type())

VALENT_AVAILABLE_IN_1_0
G_DECLARE_FINAL_TYPE (ValentContactCache, valent_contact_cache, VALENT, CONTACT_CACHE, ValentContactStore)

G_END_DECLS
