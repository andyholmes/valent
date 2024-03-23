// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SHARE_DIALOG (valent_share_dialog_get_type())

G_DECLARE_FINAL_TYPE (ValentShareDialog, valent_share_dialog, VALENT, SHARE_DIALOG, AdwWindow)

G_END_DECLS

