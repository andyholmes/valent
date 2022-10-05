// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_UI_INSIDE) && !defined (VALENT_UI_COMPILATION)
# error "Only <libvalent-core.h> can be included directly."
#endif

#include <libvalent-core.h>

G_BEGIN_DECLS

VALENT_AVAILABLE_IN_1_0
char * valent_string_to_markup (const char *text);

G_END_DECLS
