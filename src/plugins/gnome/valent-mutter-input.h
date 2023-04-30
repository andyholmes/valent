// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MUTTER_INPUT (valent_mutter_input_get_type())

G_DECLARE_FINAL_TYPE (ValentMutterInput, valent_mutter_input, VALENT, MUTTER_INPUT, ValentInputAdapter)

G_END_DECLS

