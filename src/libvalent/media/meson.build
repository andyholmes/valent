# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_media_header_dir = join_paths(libvalent_header_dir, 'media')
libvalent_media_header_subdir = join_paths(libvalent_header_subdir, 'media')
libvalent_media_generated_headers = []
libvalent_media_generated_sources = []


# Headers
libvalent_media_public_headers = [
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


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_media_public_sources)
libvalent_public_headers += files(libvalent_media_public_headers)
libvalent_private_headers += files(libvalent_media_private_headers)
libvalent_generated_headers += libvalent_media_generated_headers
libvalent_generated_sources += libvalent_media_generated_sources

