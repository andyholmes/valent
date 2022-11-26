// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-device-gadget"

#include "config.h"

#include <libvalent-ui.h>

#include "valent-mock-device-gadget.h"


struct _ValentMockDeviceGadget
{
  ValentDeviceGadget  parent_instance;
};

G_DEFINE_TYPE (ValentMockDeviceGadget, valent_mock_device_gadget, VALENT_TYPE_DEVICE_GADGET)


/*
 * GObject
 */
static void
valent_mock_device_gadget_class_init (ValentMockDeviceGadgetClass *klass)
{
}

static void
valent_mock_device_gadget_init (ValentMockDeviceGadget *self)
{
}

