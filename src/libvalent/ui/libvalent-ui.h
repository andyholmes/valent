// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libvalent-core.h>

G_BEGIN_DECLS

#define VALENT_UI_INSIDE

#include "valent-application.h"
#include "valent-application-plugin.h"
#include "valent-device-activity.h"
#include "valent-device-gadget.h"
#include "valent-device-preferences-page.h"
#include "valent-preferences-page.h"
#include "valent-ui-utils.h"

#undef VALENT_UI_INSIDE

G_END_DECLS

