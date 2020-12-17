#include <libpeas/peas.h>
#include <libvalent-core.h>
#include <libvalent-clipboard.h>
#include <libvalent-contacts.h>
#include <libvalent-input.h>
#include <libvalent-media.h>
#include <libvalent-mixer.h>
#include <libvalent-notifications.h>
#include <libvalent-power.h>

#include "valent-test-channel-service.h"
#include "valent-test-clipboard.h"
#include "valent-test-contact-store-provider.h"
#include "valent-test-input-controller.h"
#include "valent-test-mixer-control.h"
#include "valent-test-media-player-provider.h"
#include "valent-test-notification-source.h"
#include "valent-test-plugin.h"
#include "valent-test-power-device-provider.h"


G_MODULE_EXPORT void
valent_test_plugin_register_types (PeasObjectModule *module)
{
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CHANNEL_SERVICE,
                                              VALENT_TYPE_TEST_CHANNEL_SERVICE);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CLIPBOARD_SOURCE,
                                              VALENT_TYPE_TEST_CLIPBOARD);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_CONTACT_STORE_PROVIDER,
                                              VALENT_TYPE_TEST_CONTACT_STORE_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_INPUT_CONTROLLER,
                                              VALENT_TYPE_TEST_INPUT_CONTROLLER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MIXER_CONTROL,
                                              VALENT_TYPE_TEST_MIXER_CONTROL);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_MEDIA_PLAYER_PROVIDER,
                                              VALENT_TYPE_TEST_MEDIA_PLAYER_PROVIDER);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_NOTIFICATION_SOURCE,
                                              VALENT_TYPE_TEST_NOTIFICATION_SOURCE);
  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_POWER_DEVICE_PROVIDER,
                                              VALENT_TYPE_TEST_POWER_DEVICE_PROVIDER);

  peas_object_module_register_extension_type (module,
                                              VALENT_TYPE_DEVICE_PLUGIN,
                                              VALENT_TYPE_TEST_PLUGIN);
}

