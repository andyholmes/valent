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
  'valent-component.h',
  'valent-data.h',
  'valent-global.h',
  'valent-macros.h',
  'valent-object.h',
  'valent-transfer.h',
]

libvalent_core_private_headers = [
  'valent-component-private.h',
]

libvalent_core_enum_headers = [
  'valent-transfer.h',
]

install_headers(libvalent_core_public_headers,
  subdir: libvalent_core_header_subdir,
)


# Sources
libvalent_core_public_sources = [
  'valent-certificate.c',
  'valent-component.c',
  'valent-data.c',
  'valent-debug.c',
  'valent-global.c',
  'valent-object.c',
  'valent-transfer.c',
  'valent-version.c',
]


# Versioning
version_data = configuration_data()
version_data.set('MAJOR_VERSION', MAJOR_VERSION)
version_data.set('MINOR_VERSION', MINOR_VERSION)
version_data.set('MICRO_VERSION', MICRO_VERSION)
version_data.set('VERSION', meson.project_version())
version_data.set('API_VERSION', '@0@.@1@'.format(MAJOR_VERSION, MINOR_VERSION))

libvalent_core_version_h = configure_file(
          input: 'valent-version.h.in',
         output: 'valent-version.h',
  configuration: version_data,
        install: true,
    install_dir: libvalent_core_header_dir,
)

libvalent_core_generated_headers += [libvalent_core_version_h]

libvalent_core_vcs_h = vcs_tag(
      command: ['git', 'rev-parse', '--short', 'HEAD'],
     fallback: 'devel',
        input: 'valent-version-vcs.h.in',
       output: 'valent-version-vcs.h',
)

libvalent_core_generated_headers += [libvalent_core_vcs_h]


# Debugging and Profiling Support
libvalent_core_conf = configuration_data()
libvalent_core_conf.set10('VALENT_ENABLE_DEBUG', get_option('debug'))
libvalent_core_conf.set10('VALENT_ENABLE_TRACE', get_option('tracing'))

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
        header_prefix: '#include "valent-version.h"',
            decorator: '_VALENT_EXTERN',
              sources: libvalent_core_enum_headers,
       install_header: true,
          install_dir: libvalent_core_header_dir,
)
libvalent_core_generated_sources += [libvalent_core_enums[0]]
libvalent_core_generated_headers += [libvalent_core_enums[1]]


# Dependencies
libvalent_core_deps = [
  libm_dep,
  gio_dep,
  gio_unix_dep,
  gnutls_dep,
  libpeas_dep,
]

if get_option('tracing') and libsysprof_capture.found()
  libvalent_c_args += ['-DHAVE_SYSPROF']
  libvalent_core_deps += [libsysprof_capture]
endif


# Library Definitions
libvalent_core = static_library('valent-core-' + valent_api_version,
                                libvalent_core_public_sources,
                                libvalent_core_generated_sources,
                                libvalent_core_generated_headers,
               c_args: libvalent_c_args + release_args + ['-DVALENT_CORE_COMPILATION'],
         dependencies: libvalent_core_deps,
  include_directories: [config_h_inc, include_directories('.')],
)

libvalent_core_dep = declare_dependency(
              sources: libvalent_core_private_headers + libvalent_core_generated_headers,
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
libvalent_generated_sources += libvalent_core_generated_sources
libvalent_pkgconfig_subdirs += libvalent_core_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-core.h', '-DVALENT_CORE_COMPILATION']

