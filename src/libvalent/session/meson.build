# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_session_header_dir = join_paths(libvalent_header_dir, 'session')
libvalent_session_header_subdir = join_paths(libvalent_header_subdir, 'session')
libvalent_include_directories += include_directories('.')
libvalent_session_generated_headers = []
libvalent_session_generated_sources = []


# Headers
libvalent_session_public_headers = [
  'libvalent-session.h',
  'valent-session.h',
  'valent-session-adapter.h',
]

libvalent_session_private_headers = [
]

libvalent_session_enum_headers = [
]

install_headers(libvalent_session_public_headers,
  subdir: libvalent_session_header_subdir
)


# Controllers
libvalent_session_public_sources = [
  'valent-session.c',
  'valent-session-adapter.c',
]


# Dependencies
libvalent_session_deps = [
  libvalent_core_dep,
]


# Library Definitions
libvalent_session = static_library('valent-session-' + valent_api_version,
                                   libvalent_session_public_sources,
                                   libvalent_session_generated_sources,
                                   libvalent_session_generated_headers,
  dependencies: libvalent_session_deps,
        c_args: libvalent_c_args + release_args + ['-DVALENT_SESSION_COMPILATION'],
)

libvalent_session_dep = declare_dependency(
              sources: libvalent_session_private_headers,
         dependencies: libvalent_session_deps,
            link_with: libvalent_session,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_session]
libvalent_deps += [libvalent_session_dep]
libvalent_public_sources += files(libvalent_session_public_sources)
libvalent_public_headers += files(libvalent_session_public_headers)
libvalent_private_headers += files(libvalent_session_private_headers)
libvalent_generated_headers += libvalent_session_generated_headers
libvalent_generated_sources += libvalent_session_generated_sources
libvalent_include_subdirs += libvalent_session_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-session.h', '-DVALENT_SESSION_COMPILATION']

