// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_EBOOK_ADAPTER (valent_ebook_adapter_get_type ())

G_DECLARE_FINAL_TYPE (ValentEBookAdapter, valent_ebook_adapter, VALENT, EBOOK_ADAPTER, ValentContactsAdapter)

G_END_DECLS

