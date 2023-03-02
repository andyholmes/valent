// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_TELEPHONY (valent_telephony_get_type ())

G_DECLARE_FINAL_TYPE (ValentTelephony, valent_telephony, VALENT, TELEPHONY, ValentObject)

ValentTelephony * valent_telephony_get_default          (void);
JsonNode        * valent_telephony_get_signal_strengths (ValentTelephony *telephony);

G_END_DECLS

