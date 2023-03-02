# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_clipboard_header_dir = join_paths(libvalent_header_dir, 'clipboard')
libvalent_clipboard_header_subdir = join_paths(libvalent_header_subdir, 'clipboard')
libvalent_clipboard_generated_headers = []
libvalent_clipboard_generated_sources = []


# Headers
libvalent_clipboard_public_headers = [
  'valent-clipboard.h',
  'valent-clipboard-adapter.h',
]

libvalent_clipboard_private_headers = [
]

libvalent_clipboard_enum_headers = [
]

install_headers(libvalent_clipboard_public_headers,
  subdir: libvalent_clipboard_header_subdir,
)


# Sources
libvalent_clipboard_public_sources = [
  'valent-clipboard.c',
  'valent-clipboard-adapter.c',
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_clipboard_public_sources)
libvalent_public_headers += files(libvalent_clipboard_public_headers)
libvalent_private_headers += files(libvalent_clipboard_private_headers)
libvalent_generated_headers += libvalent_clipboard_generated_headers
libvalent_generated_sources += libvalent_clipboard_generated_sources

