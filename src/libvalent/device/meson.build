# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_device_header_dir = join_paths(libvalent_header_dir, 'device')
libvalent_device_header_subdir = join_paths(libvalent_header_subdir, 'device')
libvalent_include_directories += include_directories('.')
libvalent_device_generated_headers = []
libvalent_device_generated_sources = []


# Headers
libvalent_device_public_headers = [
  'libvalent-device.h',
  'valent-channel.h',
  'valent-channel-service.h',
  'valent-device.h',
  'valent-device-manager.h',
  'valent-device-plugin.h',
  'valent-device-transfer.h',
  'valent-packet.h',
]

libvalent_device_private_headers = [
  'valent-device-impl.h',
  'valent-device-private.h',
]

libvalent_device_enum_headers = [
  'valent-device.h',
]

install_headers(libvalent_device_public_headers,
  subdir: libvalent_device_header_subdir
)


# Sources
libvalent_device_public_sources = [
  'valent-channel.c',
  'valent-channel-service.c',
  'valent-device.c',
  'valent-device-impl.c',
  'valent-device-manager.c',
  'valent-device-plugin.c',
  'valent-device-transfer.c',
  'valent-packet.c',
]


# Enumerations
libvalent_device_enums = gnome.mkenums_simple('valent-device-enums',
          body_prefix: '#include "config.h"',
        header_prefix: '#include <libvalent-core.h>',
            decorator: '_VALENT_EXTERN',
              sources: libvalent_device_enum_headers,
       install_header: true,
          install_dir: libvalent_device_header_dir,
)
libvalent_device_generated_sources += [libvalent_device_enums[0]]
libvalent_device_generated_headers += [libvalent_device_enums[1]]


# Dependencies
libvalent_device_deps = [
  libvalent_core_dep,

  jsonglib_dep,
]

# Library Definitions
libvalent_device = static_library('valent-device-' + valent_api_version,
                                  libvalent_device_public_sources,
                                  libvalent_device_generated_sources,
                                  libvalent_device_generated_headers,
         dependencies: libvalent_device_deps,
               c_args: libvalent_c_args + release_args + ['-DVALENT_DEVICE_COMPILATION'],
                  pic: true,
  include_directories: [config_h_inc, include_directories('.')],
)

libvalent_device_dep = declare_dependency(
              sources: libvalent_device_private_headers + libvalent_device_generated_headers,
         dependencies: libvalent_device_deps,
            link_with: libvalent_device,
  include_directories: [config_h_inc, include_directories('.')],
)


libvalent_static += [libvalent_device]
libvalent_deps += [libvalent_device_dep]
libvalent_public_sources += files(libvalent_device_public_sources)
libvalent_public_headers += files(libvalent_device_public_headers)
libvalent_private_headers += files(libvalent_device_private_headers)
libvalent_generated_headers += libvalent_device_generated_headers
libvalent_generated_sources += libvalent_device_generated_sources
libvalent_include_subdirs += libvalent_device_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-device.h', '-DVALENT_DEVICE_COMPILATION']

