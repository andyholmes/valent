// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <gio/gio.h>
#include <libvalent-core.h>
#include <libvalent-contacts.h>

G_BEGIN_DECLS

#define VALENT_TYPE_EBOOK_STORE (valent_ebook_store_get_type())

G_DECLARE_FINAL_TYPE (ValentEBookStore, valent_ebook_store, VALENT, EBOOK_STORE, ValentContactStore)

G_END_DECLS
