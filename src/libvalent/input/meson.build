# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_input_header_dir = join_paths(libvalent_header_dir, 'input')
libvalent_input_header_subdir = join_paths(libvalent_header_subdir, 'input')
libvalent_input_generated_headers = []
libvalent_input_generated_sources = []


# Headers
libvalent_input_public_headers = [
  'valent-input.h',
  'valent-input-adapter.h',
  'valent-input-keydef.h',
]

libvalent_input_private_headers = [
]

libvalent_input_enum_headers = [
]

install_headers(libvalent_input_public_headers,
  subdir: libvalent_input_header_subdir,
)


# Sources
libvalent_input_public_sources = [
  'valent-input.c',
  'valent-input-adapter.c',
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_input_public_sources)
libvalent_public_headers += files(libvalent_input_public_headers)
libvalent_private_headers += files(libvalent_input_private_headers)
libvalent_generated_headers += libvalent_input_generated_headers
libvalent_generated_sources += libvalent_input_generated_sources

