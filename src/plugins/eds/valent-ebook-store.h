// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_EBOOK_STORE (valent_ebook_store_get_type())

G_DECLARE_FINAL_TYPE (ValentEBookStore, valent_ebook_store, VALENT, EBOOK_STORE, ValentObject)

G_END_DECLS
