// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#include <valent.h>

G_BEGIN_DECLS

#define VALENT_TYPE_MOCK_DEVICE_GADGET (valent_mock_device_gadget_get_type())

G_DECLARE_FINAL_TYPE (ValentMockDeviceGadget, valent_mock_device_gadget, VALENT, MOCK_DEVICE_GADGET, ValentDeviceGadget)

G_END_DECLS
