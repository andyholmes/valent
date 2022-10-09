// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <libvalent-ui.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SHARE_TARGET (valent_share_target_get_type())

G_DECLARE_FINAL_TYPE (ValentShareTarget, valent_share_target, VALENT, SHARE_TARGET, ValentApplicationPlugin)

G_END_DECLS

