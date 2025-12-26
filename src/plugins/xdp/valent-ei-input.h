// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_EI_INPUT (valent_ei_input_get_type ())

G_DECLARE_FINAL_TYPE (ValentEiInput, valent_ei_input, VALENT, EI_INPUT, ValentInputAdapter)

G_END_DECLS

