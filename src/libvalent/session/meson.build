# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_session_header_dir = join_paths(libvalent_header_dir, 'session')
libvalent_session_header_subdir = join_paths(libvalent_header_subdir, 'session')
libvalent_session_generated_headers = []
libvalent_session_generated_sources = []


# Headers
libvalent_session_public_headers = [
  'valent-session.h',
  'valent-session-adapter.h',
]

libvalent_session_private_headers = [
]

libvalent_session_enum_headers = [
]

install_headers(libvalent_session_public_headers,
  subdir: libvalent_session_header_subdir,
)


# Sources
libvalent_session_public_sources = [
  'valent-session.c',
  'valent-session-adapter.c',
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_session_public_sources)
libvalent_public_headers += files(libvalent_session_public_headers)
libvalent_private_headers += files(libvalent_session_private_headers)
libvalent_generated_headers += libvalent_session_generated_headers
libvalent_generated_sources += libvalent_session_generated_sources

