// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define VALENT_TYPE_DEVICE_PAGE (valent_device_page_get_type())

G_DECLARE_FINAL_TYPE (ValentDevicePage, valent_device_page, VALENT, DEVICE_PAGE, AdwNavigationPage)

G_END_DECLS
