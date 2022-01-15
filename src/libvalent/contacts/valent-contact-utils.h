// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <glib.h>

#include "valent-eds.h"

G_BEGIN_DECLS

gboolean   valent_phone_number_equal              (const char  *number1,
                                                   const char  *number2);
char     * valent_phone_number_normalize          (const char  *number);
gboolean   valent_phone_number_of_contact         (EContact    *contact,
                                                   const char  *number);

G_END_DECLS
