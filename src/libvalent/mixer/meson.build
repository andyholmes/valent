# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_mixer_header_dir = join_paths(libvalent_header_dir, 'mixer')
libvalent_mixer_header_subdir = join_paths(libvalent_header_subdir, 'mixer')
libvalent_include_directories += include_directories('.')
libvalent_mixer_generated_headers = []
libvalent_mixer_generated_sources = []


# Headers
libvalent_mixer_public_headers = [
  'libvalent-mixer.h',
  'valent-mixer.h',
  'valent-mixer-adapter.h',
  'valent-mixer-stream.h',
]

libvalent_mixer_private_headers = [
]

libvalent_mixer_enum_headers = [
  'valent-mixer-stream.h',
]

install_headers(libvalent_mixer_public_headers,
  subdir: libvalent_mixer_header_subdir
)


# Sources
libvalent_mixer_public_sources = [
  'valent-mixer.c',
  'valent-mixer-adapter.c',
  'valent-mixer-stream.c',
]


# Enumerations
libvalent_mixer_enums = gnome.mkenums_simple('valent-mixer-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libvalent-core.h>',
       decorator: '_VALENT_EXTERN',
         sources: libvalent_mixer_enum_headers,
  install_header: true,
     install_dir: libvalent_mixer_header_dir,
)
libvalent_mixer_generated_sources += [libvalent_mixer_enums[0]]
libvalent_mixer_generated_headers += [libvalent_mixer_enums[1]]


# Dependencies
libvalent_mixer_deps = [
  libvalent_core_dep,
]


# Library Definitions
libvalent_mixer = static_library('valent-mixer-' + valent_api_version,
                                 libvalent_mixer_public_sources,
                                 libvalent_mixer_generated_sources,
                                 libvalent_mixer_generated_headers,
  dependencies: libvalent_mixer_deps,
        c_args: libvalent_c_args + release_args + ['-DVALENT_MIXER_COMPILATION'],
           pic: true,
)

libvalent_mixer_dep = declare_dependency(
              sources: [libvalent_mixer_generated_headers, libvalent_mixer_private_headers],
         dependencies: libvalent_mixer_deps,
            link_with: libvalent_mixer,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_mixer]
libvalent_deps += [libvalent_mixer_dep]
libvalent_public_sources += files(libvalent_mixer_public_sources)
libvalent_public_headers += files(libvalent_mixer_public_headers)
libvalent_private_headers += files(libvalent_mixer_private_headers)
libvalent_generated_headers += libvalent_mixer_generated_headers
libvalent_generated_sources += libvalent_mixer_generated_sources
libvalent_include_subdirs += libvalent_mixer_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-mixer.h', '-DVALENT_MIXER_COMPILATION']

