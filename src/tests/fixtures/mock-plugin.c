#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>
#include <libvalent-contacts.h>
#include <libvalent-input.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>
#include <libvalent-notifications.h>
#include <libvalent-power.h>

#include "valent-mock-channel-service.h"
#include "valent-mock-clipboard-source.h"
#include "valent-mock-contact-store-provider.h"
#include "valent-mock-input-controller.h"
#include "valent-mock-mixer-control.h"
#include "valent-mock-media-player-provider.h"
#include "valent-mock-notification-source.h"
#include "valent-mock-device-plugin.h"
#include "valent-mock-power-device-provider.h"


G_MODULE_EXPORT void
valent_mock_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CHANNEL_SERVICE,
                                              VALENT_TYPE_MOCK_CHANNEL_SERVICE);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CLIPBOARD_SOURCE,
                                              VALENT_TYPE_MOCK_CLIPBOARD_SOURCE);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CONTACT_STORE_PROVIDER,
                                              VALENT_TYPE_MOCK_CONTACT_STORE_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_INPUT_CONTROLLER,
                                              VALENT_TYPE_MOCK_INPUT_CONTROLLER);
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
                                              VALENT_TYPE_POWER_DEVICE_PROVIDER,
                                              VALENT_TYPE_MOCK_POWER_DEVICE_PROVIDER);

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_MOCK_DEVICE_PLUGIN);
}

