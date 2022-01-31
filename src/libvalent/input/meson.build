# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_input_header_dir = join_paths(libvalent_header_dir, 'input')
libvalent_input_header_subdir = join_paths(libvalent_header_subdir, 'input')
libvalent_include_directories += include_directories('.')
libvalent_input_generated_headers = []
libvalent_input_generated_sources = []


# Headers
libvalent_input_public_headers = [
  'libvalent-input.h',
  'valent-input.h',
  'valent-input-adapter.h',
  'valent-input-keydef.h',
]

libvalent_input_private_headers = [
]

libvalent_input_enum_headers = [
  'valent-input-keydef.h',
]

install_headers(libvalent_input_public_headers,
  subdir: libvalent_input_header_subdir
)


# Sources
libvalent_input_public_sources = [
  'valent-input.c',
  'valent-input-adapter.c',
]


# Enumerations
libvalent_input_enums = gnome.mkenums_simple('valent-input-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libvalent-core.h>',
       decorator: '_VALENT_EXTERN',
         sources: libvalent_input_enum_headers,
  install_header: true,
     install_dir: libvalent_input_header_dir,
)
libvalent_input_generated_sources += [libvalent_input_enums[0]]
libvalent_input_generated_headers += [libvalent_input_enums[1]]


# Dependencies
libvalent_input_deps = [
  libvalent_core_dep,

  gtk_dep,
]


# Library Definitions
libvalent_input = static_library('valent-input-' + valent_api_version,
                                 libvalent_input_public_sources,
                                 libvalent_input_generated_sources,
                                 libvalent_input_generated_headers,
  dependencies: libvalent_input_deps,
        c_args: libvalent_c_args + release_args + ['-DVALENT_INPUT_COMPILATION'],
           pic: true,
)

libvalent_input_dep = declare_dependency(
              sources: libvalent_input_private_headers,
         dependencies: libvalent_input_deps,
            link_with: libvalent_input,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_input]
libvalent_deps += [libvalent_input_dep]
libvalent_public_sources += files(libvalent_input_public_sources)
libvalent_public_headers += files(libvalent_input_public_headers)
libvalent_private_headers += files(libvalent_input_private_headers)
libvalent_generated_headers += libvalent_input_generated_headers
libvalent_generated_sources += libvalent_input_generated_sources
libvalent_include_subdirs += libvalent_input_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-input.h', '-DVALENT_INPUT_COMPILATION']

