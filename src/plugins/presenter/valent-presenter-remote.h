// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_PRESENTER_REMOTE (valent_presenter_remote_get_type())

G_DECLARE_FINAL_TYPE (ValentPresenterRemote, valent_presenter_remote, VALENT, PRESENTER_REMOTE, AdwWindow)

G_END_DECLS
