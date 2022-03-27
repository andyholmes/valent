// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>
#include <libvalent-contacts.h>
#include <libvalent-input.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>
#include <libvalent-notifications.h>
#include <libvalent-session.h>
#include <libvalent-ui.h>

#include "valent-mock-channel-service.h"
#include "valent-mock-clipboard-adapter.h"
#include "valent-mock-contact-store-provider.h"
#include "valent-mock-input-adapter.h"
#include "valent-mock-mixer-control.h"
#include "valent-mock-media-player-provider.h"
#include "valent-mock-notification-source.h"
#include "valent-mock-preferences.h"
#include "valent-mock-device-gadget.h"
#include "valent-mock-device-plugin.h"
#include "valent-mock-session-adapter.h"


G_MODULE_EXPORT void
valent_mock_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CHANNEL_SERVICE,
                                              VALENT_TYPE_MOCK_CHANNEL_SERVICE);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CLIPBOARD_ADAPTER,
                                              VALENT_TYPE_MOCK_CLIPBOARD_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CONTACT_STORE_PROVIDER,
                                              VALENT_TYPE_MOCK_CONTACT_STORE_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_INPUT_ADAPTER,
                                              VALENT_TYPE_MOCK_INPUT_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MIXER_CONTROL,
                                              VALENT_TYPE_MOCK_MIXER_CONTROL);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MEDIA_PLAYER_PROVIDER,
                                              VALENT_TYPE_MOCK_MEDIA_PLAYER_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_NOTIFICATION_SOURCE,
                                              VALENT_TYPE_MOCK_NOTIFICATION_SOURCE);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_SESSION_ADAPTER,
                                              VALENT_TYPE_MOCK_SESSION_ADAPTER);

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_MOCK_DEVICE_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PREFERENCES_PAGE,
                                              VALENT_TYPE_MOCK_PREFERENCES);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_GADGET,
                                              VALENT_TYPE_MOCK_DEVICE_GADGET);
}

