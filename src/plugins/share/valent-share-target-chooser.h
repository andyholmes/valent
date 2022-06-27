// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VALENT_TYPE_SHARE_TARGET_CHOOSER (valent_share_target_chooser_get_type())

G_DECLARE_FINAL_TYPE (ValentShareTargetChooser, valent_share_target_chooser, VALENT, SHARE_TARGET_CHOOSER, GtkWindow)

G_END_DECLS

