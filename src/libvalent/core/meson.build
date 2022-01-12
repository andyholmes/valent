# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_core_header_dir = join_paths(libvalent_header_dir, 'core')
libvalent_core_header_subdir = join_paths(libvalent_header_subdir, 'core')
libvalent_include_directories += include_directories('.')
libvalent_core_generated_headers = []
libvalent_core_generated_sources = []


# Headers
libvalent_core_public_headers = [
  'libvalent-core.h',
  'valent-certificate.h',
  'valent-channel.h',
  'valent-channel-service.h',
  'valent-component.h',
  'valent-data.h',
  'valent-device.h',
  'valent-device-plugin.h',
  'valent-macros.h',
  'valent-manager.h',
  'valent-object.h',
  'valent-packet.h',
  'valent-task-queue.h',
  'valent-transfer.h',
  'valent-utils.h',
]

libvalent_core_private_headers = [
  'valent-device-impl.h',
  'valent-device-private.h',
]

libvalent_core_enum_headers = [
  'valent-device.h',
  'valent-transfer.h',
]

install_headers(libvalent_core_public_headers,
  subdir: libvalent_core_header_subdir
)


# Sources
libvalent_core_public_sources = [
  'valent-certificate.c',
  'valent-channel.c',
  'valent-channel-service.c',
  'valent-component.c',
  'valent-data.c',
  'valent-debug.c',
  'valent-device.c',
  'valent-device-impl.c',
  'valent-device-plugin.c',
  'valent-manager.c',
  'valent-object.c',
  'valent-object-utils.c',
  'valent-packet.c',
  'valent-task-queue.c',
  'valent-transfer.c',
  'valent-utils.c',
]


# Debugging and Profiling Support
libvalent_core_conf = configuration_data()
libvalent_core_conf.set10('VALENT_ENABLE_DEBUG', get_option('debug'))
libvalent_core_conf.set10('VALENT_ENABLE_PROFILING', get_option('profiling'))

libvalent_debug_h = configure_file(
          input: 'valent-debug.h.in',
         output: 'valent-debug.h',
  configuration: libvalent_core_conf,
        install: true,
    install_dir: libvalent_core_header_dir,
)

libvalent_core_generated_headers += [libvalent_debug_h]


# Enumerations
libvalent_core_enums = gnome.mkenums_simple('valent-core-enums',
          body_prefix: '#include "config.h"',
        header_prefix: '#include <libvalent-core.h>',
              sources: libvalent_core_enum_headers,
       install_header: true,
          install_dir: libvalent_core_header_dir,
  include_directories: [config_h_inc, include_directories('.')],
)
libvalent_core_generated_sources += [libvalent_core_enums[0]]
libvalent_core_generated_headers += [libvalent_core_enums[1]]


# Dependencies
libvalent_core_deps = [
  libm_dep,
  gio_dep,
  gio_unix_dep,
  jsonglib_dep,
  libpeas_dep,

  gnutls_dep,
]

if get_option('profiling')
  libvalent_core_deps += [libsysprof_capture]
endif


# Library Definitions
libvalent_core = static_library('valent-core-' + valent_api_version,
                                libvalent_core_public_sources,
                                libvalent_core_generated_sources,
                                libvalent_core_generated_headers,
         dependencies: libvalent_core_deps,
               c_args: libvalent_c_args + release_args + ['-DVALENT_CORE_COMPILATION'],
                  pic: true,
  include_directories: [config_h_inc, include_directories('.')],
)

libvalent_core_dep = declare_dependency(
              sources: libvalent_core_private_headers,
         dependencies: libvalent_core_deps,
            link_with: libvalent_core,
  include_directories: [config_h_inc, include_directories('.')],
)


libvalent_static += [libvalent_core]
libvalent_deps += [libvalent_core_dep]
libvalent_public_sources += files(libvalent_core_public_sources)
libvalent_public_headers += files(libvalent_core_public_headers)
libvalent_private_headers += files(libvalent_core_private_headers)
libvalent_generated_headers += libvalent_core_generated_headers
libvalent_include_subdirs += libvalent_core_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-core.h', '-DVALENT_CORE_COMPILATION']

