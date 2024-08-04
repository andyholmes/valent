# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_messages_header_dir = join_paths(libvalent_header_dir, 'messages')
libvalent_messages_header_subdir = join_paths(libvalent_header_subdir, 'messages')
libvalent_messages_generated_headers = []
libvalent_messages_generated_sources = []


# Headers
libvalent_messages_public_headers = [
  'valent-messages.h',
  'valent-messages-adapter.h',
  'valent-message.h',
  'valent-message-attachment.h',
]

libvalent_messages_private_headers = [
  'valent-messages-adapter-private.h',
  'valent-message-thread.h',
]

libvalent_messages_enum_headers = [
  'valent-message.h',
]

install_headers(libvalent_messages_public_headers,
  subdir: libvalent_messages_header_subdir,
)


# Sources
libvalent_messages_public_sources = [
  'valent-messages.c',
  'valent-messages-adapter.c',
  'valent-message.c',
  'valent-message-attachment.c',
  'valent-message-thread.c',
]


# Enumerations
libvalent_messages_enums = gnome.mkenums_simple('valent-messages-enums',
     body_prefix: '#include "config.h"',
   header_prefix: '#include "../core/valent-version.h"',
       decorator: '_VALENT_EXTERN',
         sources: libvalent_messages_enum_headers,
  install_header: true,
     install_dir: libvalent_messages_header_dir,
)
libvalent_messages_generated_sources += [libvalent_messages_enums[0]]
libvalent_messages_generated_headers += [libvalent_messages_enums[1]]


# GResources
libvalent_messages_resources = gnome.compile_resources('libvalent-messages-resources',
                                                       'libvalent-messages.gresource.xml',
  c_name: 'valent_messages',
)
libvalent_messages_generated_sources += libvalent_messages_resources
libvalent_messages_generated_headers += [libvalent_messages_resources[1]]


# Dependencies
libvalent_deps += [
  tinysparql_dep,
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_messages_public_sources)
libvalent_public_headers += files(libvalent_messages_public_headers)
libvalent_private_headers += files(libvalent_messages_private_headers)
libvalent_generated_headers += libvalent_messages_generated_headers
libvalent_generated_sources += libvalent_messages_generated_sources
