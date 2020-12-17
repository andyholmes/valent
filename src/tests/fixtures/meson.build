# Generated Resource Files
libvalent_test_resources = gnome.compile_resources(
  'valent-test-resources',
  'libvalent-test.gresource.xml',
  c_name: 'valent_test',
)
libvalent_test_generated_sources = [libvalent_test_resources]
libvalent_test_generated_headers = [libvalent_test_resources[1]]


# Headers
libvalent_test_public_headers = [
  'libvalent-test.h',
  'packetless-plugin.h',
  'valent-test-channel.h',
  'valent-test-channel-service.h',
  'valent-test-clipboard.h',
  'valent-test-contact-store-provider.h',
  'valent-test-input-controller.h',
  'valent-test-media-player.h',
  'valent-test-media-player-provider.h',
  'valent-test-mixer-control.h',
  'valent-test-notification-source.h',
  'valent-test-plugin.h',
  'valent-test-plugin-fixture.h',
  'valent-test-power-device.h',
  'valent-test-power-device-provider.h',
  'valent-test-utils.h',
]

libvalent_test_private_headers = [
]


# Sources
libvalent_test_public_sources = [
  'packetless-plugin.c',
  'test-plugin.c',
  'valent-test-channel.c',
  'valent-test-channel-service.c',
  'valent-test-clipboard.c',
  'valent-test-contact-store-provider.c',
  'valent-test-input-controller.c',
  'valent-test-media-player.c',
  'valent-test-media-player-provider.c',
  'valent-test-mixer-control.c',
  'valent-test-notification-source.c',
  'valent-test-plugin.c',
  'valent-test-plugin-fixture.c',
  'valent-test-power-device.c',
  'valent-test-power-device-provider.c',
  'valent-test-utils.c',
]


# Dependencies
libvalent_test_deps = [
  libvalent_core_dep,
  libvalent_clipboard_dep,
  libvalent_contacts_dep,
  libvalent_input_dep,
  libvalent_media_dep,
  libvalent_mixer_dep,
  libvalent_notifications_dep,
  libvalent_power_dep,
]


# Library Definitions
libvalent_test = static_library('valent-test-' + valent_api_version,
                                libvalent_test_public_sources,
                                libvalent_test_generated_sources,
                                libvalent_test_generated_headers,
  dependencies: libvalent_test_deps,
        c_args: libvalent_c_args + release_args + ['-DVALENT_TEST_COMPILATION'],
)

libvalent_test_dep = declare_dependency(
              sources: libvalent_test_private_headers,
         dependencies: libvalent_test_deps,
            link_with: libvalent_test,
  include_directories: include_directories('.'),
)

