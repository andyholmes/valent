// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-core.h>

#include "valent-eds.h"

G_BEGIN_DECLS

VALENT_AVAILABLE_IN_1_0
gboolean   valent_phone_number_equal      (const char *number1,
                                           const char *number2);
VALENT_AVAILABLE_IN_1_0
char     * valent_phone_number_normalize  (const char *number);
VALENT_AVAILABLE_IN_1_0
gboolean   valent_phone_number_of_contact (EContact   *contact,
                                           const char *number);

G_END_DECLS
