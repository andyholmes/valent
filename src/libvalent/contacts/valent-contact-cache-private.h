// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include "valent-contact-store.h"

G_BEGIN_DECLS

#define VALENT_TYPE_CONTACT_CACHE (valent_contact_cache_get_type())

_VALENT_EXTERN
G_DECLARE_FINAL_TYPE (ValentContactCache, valent_contact_cache, VALENT, CONTACT_CACHE, ValentContactStore)

G_END_DECLS
