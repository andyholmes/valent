// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#pragma once

#if !defined (VALENT_POWER_INSIDE) && !defined (VALENT_POWER_COMPILATION)
# error "Only <libvalent-power.h> can be included directly."
#endif

#include <gio/gio.h>

#include "valent-power-device.h"

G_BEGIN_DECLS

#define VALENT_TYPE_POWER_DEVICE_PROVIDER (valent_power_device_provider_get_type())

G_DECLARE_DERIVABLE_TYPE (ValentPowerDeviceProvider, valent_power_device_provider, VALENT, POWER_DEVICE_PROVIDER, GObject)

struct _ValentPowerDeviceProviderClass
{
  GObjectClass   parent_class;

  /* virtual functions */
  void           (*load_async)     (ValentPowerDeviceProvider  *provider,
                                    GCancellable               *cancellable,
                                    GAsyncReadyCallback         callback,
                                    gpointer                    user_data);
  gboolean       (*load_finish)    (ValentPowerDeviceProvider  *provider,
                                    GAsyncResult               *result,
                                    GError                    **error);

  /* signals */
  void           (*device_added)   (ValentPowerDeviceProvider  *provider,
                                    ValentPowerDevice          *device);
  void           (*device_removed) (ValentPowerDeviceProvider  *provider,
                                    ValentPowerDevice          *device);
};

void        valent_power_device_provider_emit_device_added   (ValentPowerDeviceProvider  *provider,
                                                              ValentPowerDevice          *device);
void        valent_power_device_provider_emit_device_removed (ValentPowerDeviceProvider  *provider,
                                                              ValentPowerDevice          *device);
GPtrArray * valent_power_device_provider_get_devices         (ValentPowerDeviceProvider  *provider);
void        valent_power_device_provider_load_async          (ValentPowerDeviceProvider  *provider,
                                                              GCancellable               *cancellable,
                                                              GAsyncReadyCallback         callback,
                                                              gpointer                    user_data);
gboolean    valent_power_device_provider_load_finish         (ValentPowerDeviceProvider  *provider,
                                                              GAsyncResult               *result,
                                                              GError                    **error);

G_END_DECLS

