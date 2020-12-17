libvalent_notifications_header_dir = join_paths(libvalent_header_dir, 'notifications')
libvalent_notifications_header_subdir = join_paths(libvalent_header_subdir, 'notifications')
libvalent_include_directories += include_directories('.')
libvalent_notifications_generated_headers = []
libvalent_notifications_generated_sources = []


# Headers
libvalent_notifications_public_headers = [
  'libvalent-notifications.h',
  'valent-notification.h',
  'valent-notification-source.h',
  'valent-notifications.h',
]

libvalent_notifications_private_headers = [
]

install_headers(libvalent_notifications_public_headers,
  subdir: libvalent_notifications_header_subdir
)


# Sources
libvalent_notifications_public_sources = [
  'valent-notification.c',
  'valent-notification-source.c',
  'valent-notifications.c',
]


# Dependencies
libvalent_notifications_deps = [
  libvalent_core_dep,
]


# Library Definitions
libvalent_notifications = static_library('valent-notifications-' + valent_api_version,
                                         libvalent_notifications_public_sources,
                                         libvalent_notifications_generated_sources,
                                         libvalent_notifications_generated_headers,
  dependencies: libvalent_notifications_deps,
        c_args: libvalent_c_args + release_args + ['-DVALENT_NOTIFICATIONS_COMPILATION'],
)

libvalent_notifications_dep = declare_dependency(
              sources: libvalent_notifications_private_headers,
         dependencies: libvalent_notifications_deps,
            link_with: libvalent_notifications,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_notifications]
libvalent_deps += [libvalent_notifications_dep]
libvalent_public_sources += files(libvalent_notifications_public_sources)
libvalent_public_headers += files(libvalent_notifications_public_headers)
libvalent_private_headers += files(libvalent_notifications_private_headers)
libvalent_generated_headers += libvalent_notifications_generated_headers
libvalent_generated_sources += libvalent_notifications_generated_sources
libvalent_include_subdirs += libvalent_notifications_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-notifications.h', '-DVALENT_NOTIFICATIONS_COMPILATION']

