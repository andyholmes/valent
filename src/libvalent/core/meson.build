# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_core_header_dir = join_paths(libvalent_header_dir, 'core')
libvalent_core_header_subdir = join_paths(libvalent_header_subdir, 'core')
libvalent_core_generated_headers = []
libvalent_core_generated_sources = []


# Headers
libvalent_core_public_headers = [
  'valent-application.h',
  'valent-application-plugin.h',
  'valent-certificate.h',
  'valent-component.h',
  'valent-context.h',
  'valent-extension.h',
  'valent-global.h',
  'valent-macros.h',
  'valent-object.h',
  'valent-transfer.h',
]

libvalent_core_private_headers = [
  'valent-component-private.h',
]

libvalent_core_enum_headers = [
  'valent-extension.h',
  'valent-transfer.h',
]

install_headers(libvalent_core_public_headers,
  subdir: libvalent_core_header_subdir,
)


# Sources
libvalent_core_public_sources = [
  'valent-application.c',
  'valent-application-plugin.c',
  'valent-certificate.c',
  'valent-component.c',
  'valent-context.c',
  'valent-extension.c',
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
version_data.set('API_VERSION', libvalent_api_version)

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
libvalent_deps += [
  libm_dep,
  gio_dep,
  gio_unix_dep,
  gnutls_dep,
  libpeas_dep,
  libportal_dep,
]

if get_option('tracing') and libsysprof_capture.found()
  libvalent_c_args += ['-DHAVE_SYSPROF']
  libvalent_deps += [libsysprof_capture]
endif


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_core_public_sources)
libvalent_public_headers += files(libvalent_core_public_headers)
libvalent_private_headers += files(libvalent_core_private_headers)
libvalent_generated_headers += libvalent_core_generated_headers
libvalent_generated_sources += libvalent_core_generated_sources

