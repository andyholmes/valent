// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>
#include <libvalent-contacts.h>

G_BEGIN_DECLS

#define VALENT_TYPE_EBOOK_PROVIDER (valent_ebook_provider_get_type ())

G_DECLARE_FINAL_TYPE (ValentEBookProvider, valent_ebook_provider, VALENT, EBOOK_PROVIDER, ValentContactStoreProvider)

G_END_DECLS

