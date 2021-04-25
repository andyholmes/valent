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
  'valent-mock-channel.h',
  'valent-mock-channel-service.h',
  'valent-mock-clipboard-source.h',
  'valent-mock-contact-store-provider.h',
  'valent-mock-device-gadget.h',
  'valent-mock-device-plugin.h',
  'valent-mock-input-controller.h',
  'valent-mock-media-player.h',
  'valent-mock-media-player-provider.h',
  'valent-mock-mixer-control.h',
  'valent-mock-notification-source.h',
  'valent-mock-session-adapter.h',
  'valent-test-plugin-fixture.h',
  'valent-test-utils.h',
]

libvalent_test_private_headers = [
]


# Sources
libvalent_test_public_sources = [
  'mock-plugin.c',
  'packetless-plugin.c',
  'valent-mock-channel.c',
  'valent-mock-channel-service.c',
  'valent-mock-clipboard-source.c',
  'valent-mock-contact-store-provider.c',
  'valent-mock-device-gadget.c',
  'valent-mock-device-plugin.c',
  'valent-mock-input-controller.c',
  'valent-mock-media-player.c',
  'valent-mock-media-player-provider.c',
  'valent-mock-mixer-control.c',
  'valent-mock-notification-source.c',
  'valent-mock-session-adapter.c',
  'valent-test-plugin-fixture.c',
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
  libvalent_session_dep,
  libvalent_ui_dep,
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

