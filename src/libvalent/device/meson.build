# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_device_header_dir = join_paths(libvalent_header_dir, 'device')
libvalent_device_header_subdir = join_paths(libvalent_header_subdir, 'device')
libvalent_device_generated_headers = []
libvalent_device_generated_sources = []


# Headers
libvalent_device_public_headers = [
  'valent-certificate.h',
  'valent-channel.h',
  'valent-channel-service.h',
  'valent-device.h',
  'valent-device-common.h',
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
  subdir: libvalent_device_header_subdir,
)


# Sources
libvalent_device_public_sources = [
  'libvalent-device.c',
  'valent-certificate.c',
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
        header_prefix: '#include "../core/valent-version.h"',
            decorator: '_VALENT_EXTERN',
              sources: libvalent_device_enum_headers,
       install_header: true,
          install_dir: libvalent_device_header_dir,
)
libvalent_device_generated_sources += [libvalent_device_enums[0]]
libvalent_device_generated_headers += [libvalent_device_enums[1]]


# GResources
libvalent_device_resources = gnome.compile_resources('libvalent-device-resources',
                                                     'libvalent-device.gresource.xml',
  c_name: 'valent_device',
)
libvalent_device_generated_sources += libvalent_device_resources
libvalent_device_generated_headers += [libvalent_device_resources[1]]


# Dependencies
libvalent_deps += [
  json_glib_dep,
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_device_public_sources)
libvalent_public_headers += files(libvalent_device_public_headers)
libvalent_private_headers += files(libvalent_device_private_headers)
libvalent_generated_headers += libvalent_device_generated_headers
libvalent_generated_sources += libvalent_device_generated_sources

