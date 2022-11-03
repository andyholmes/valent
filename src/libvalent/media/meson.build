# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_media_header_dir = join_paths(libvalent_header_dir, 'media')
libvalent_media_header_subdir = join_paths(libvalent_header_subdir, 'media')
libvalent_include_directories += include_directories('.')
libvalent_media_generated_headers = []
libvalent_media_generated_sources = []


# Headers
libvalent_media_public_headers = [
  'libvalent-media.h',
  'valent-media.h',
  'valent-media-adapter.h',
  'valent-media-player.h',
]

libvalent_media_private_headers = [
]

libvalent_media_enum_headers = [
  'valent-media-player.h',
]

install_headers(libvalent_media_public_headers,
  subdir: libvalent_media_header_subdir,
)


# Sources
libvalent_media_public_sources = [
  'valent-media.c',
  'valent-media-adapter.c',
  'valent-media-player.c',
]


# Enumerations
libvalent_media_enums = gnome.mkenums_simple('valent-media-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include <libvalent-core.h>',
       decorator: '_VALENT_EXTERN',
         sources: libvalent_media_enum_headers,
  install_header: true,
     install_dir: libvalent_media_header_dir,
)
libvalent_media_generated_sources += [libvalent_media_enums[0]]
libvalent_media_generated_headers += [libvalent_media_enums[1]]


# Dependencies
libvalent_media_deps = [
  libvalent_core_dep,
]


# Library Definitions
libvalent_media = static_library('valent-media-' + valent_api_version,
                                 libvalent_media_public_sources,
                                 libvalent_media_generated_sources,
                                 libvalent_media_generated_headers,
        c_args: libvalent_c_args + release_args + ['-DVALENT_MEDIA_COMPILATION'],
  dependencies: libvalent_media_deps,
)

libvalent_media_dep = declare_dependency(
              sources: [libvalent_media_generated_headers, libvalent_media_private_headers],
         dependencies: libvalent_media_deps,
            link_with: libvalent_media,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_media]
libvalent_deps += [libvalent_media_dep]
libvalent_public_sources += files(libvalent_media_public_sources)
libvalent_public_headers += files(libvalent_media_public_headers)
libvalent_private_headers += files(libvalent_media_private_headers)
libvalent_generated_headers += libvalent_media_generated_headers
libvalent_generated_sources += libvalent_media_generated_sources
libvalent_pkgconfig_subdirs += libvalent_media_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-media.h', '-DVALENT_MEDIA_COMPILATION']

