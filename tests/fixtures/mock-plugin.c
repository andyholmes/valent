// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

#include "config.h"

#include <libpeas.h>
#include <valent.h>

#include "valent-mock-application-plugin.h"
#include "valent-mock-channel-service.h"
#include "valent-mock-clipboard-adapter.h"
#include "valent-mock-contacts-adapter.h"
#include "valent-mock-input-adapter.h"
#include "valent-mock-media-adapter.h"
#include "valent-mock-messages-adapter.h"
#include "valent-mock-mixer-adapter.h"
#include "valent-mock-notifications-adapter.h"
#include "valent-mock-session-adapter.h"
#include "valent-mock-device-plugin.h"


_VALENT_EXTERN void
valent_mock_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_APPLICATION_PLUGIN,
                                              VALENT_TYPE_MOCK_APPLICATION_PLUGIN);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CHANNEL_SERVICE,
                                              VALENT_TYPE_MOCK_CHANNEL_SERVICE);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CLIPBOARD_ADAPTER,
                                              VALENT_TYPE_MOCK_CLIPBOARD_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CONTACTS_ADAPTER,
                                              VALENT_TYPE_MOCK_CONTACTS_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_INPUT_ADAPTER,
                                              VALENT_TYPE_MOCK_INPUT_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MEDIA_ADAPTER,
                                              VALENT_TYPE_MOCK_MEDIA_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MESSAGES_ADAPTER,
                                              VALENT_TYPE_MOCK_MESSAGES_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MIXER_ADAPTER,
                                              VALENT_TYPE_MOCK_MIXER_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_NOTIFICATIONS_ADAPTER,
                                              VALENT_TYPE_MOCK_NOTIFICATIONS_ADAPTER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_SESSION_ADAPTER,
                                              VALENT_TYPE_MOCK_SESSION_ADAPTER);

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_MOCK_DEVICE_PLUGIN);
}

