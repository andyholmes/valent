// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#define G_LOG_DOMAIN "valent-mock-power-device-provider"

#include "config.h"

#include <libvalent-core.h>
#include <libvalent-power.h>

#include "valent-mock-power-device.h"
#include "valent-mock-power-device-provider.h"


struct _ValentMockPowerDeviceProvider
{
  ValentPowerDeviceProvider  parent_instance;
};

G_DEFINE_TYPE (ValentMockPowerDeviceProvider, valent_mock_power_device_provider, VALENT_TYPE_POWER_DEVICE_PROVIDER)


/*
 * ValentPowerDeviceProvider
 */
static void
valent_mock_power_device_provider_load_async (ValentPowerDeviceProvider *provider,
                                              GCancellable              *cancellable,
                                              GAsyncReadyCallback        callback,
                                              gpointer                   user_data)
{
  g_autoptr (GTask) task = NULL;

  g_assert (VALENT_IS_MOCK_POWER_DEVICE_PROVIDER (provider));
  g_assert (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (provider, cancellable, callback, user_data);
  g_task_set_source_tag (task, valent_mock_power_device_provider_load_async);
  g_task_return_boolean (task, TRUE);
}

/*
 * GObject
 */
static void
valent_mock_power_device_provider_class_init (ValentMockPowerDeviceProviderClass *klass)
{
  ValentPowerDeviceProviderClass *provider_class = VALENT_POWER_DEVICE_PROVIDER_CLASS (klass);

  provider_class->load_async = valent_mock_power_device_provider_load_async;
}

static void
valent_mock_power_device_provider_init (ValentMockPowerDeviceProvider *self)
{
}

