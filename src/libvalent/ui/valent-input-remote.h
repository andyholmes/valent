// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

#include "../core/valent-object.h"

G_BEGIN_DECLS

#define VALENT_TYPE_INPUT_REMOTE (valent_input_remote_get_type())

G_DECLARE_FINAL_TYPE (ValentInputRemote, valent_input_remote, VALENT, INPUT_REMOTE, AdwWindow)

G_END_DECLS
