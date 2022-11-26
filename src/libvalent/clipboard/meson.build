# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_clipboard_header_dir = join_paths(libvalent_header_dir, 'clipboard')
libvalent_clipboard_header_subdir = join_paths(libvalent_header_subdir, 'clipboard')
libvalent_include_directories += include_directories('.')
libvalent_clipboard_generated_headers = []
libvalent_clipboard_generated_sources = []


# Headers
libvalent_clipboard_public_headers = [
  'libvalent-clipboard.h',
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


# Dependencies
libvalent_clipboard_deps = [
  libvalent_core_dep,
]


# Library Definitions
libvalent_clipboard = static_library('valent-clipboard-@0@'.format(libvalent_api_version),
                                     libvalent_clipboard_public_sources,
                                     libvalent_clipboard_generated_sources,
                                     libvalent_clipboard_generated_headers,
        c_args: libvalent_c_args + release_args + ['-DVALENT_CLIPBOARD_COMPILATION'],
  dependencies: libvalent_clipboard_deps,
)

libvalent_clipboard_dep = declare_dependency(
              sources: [libvalent_clipboard_generated_headers, libvalent_clipboard_private_headers],
         dependencies: libvalent_clipboard_deps,
            link_with: libvalent_clipboard,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_clipboard]
libvalent_deps += [libvalent_clipboard_dep]
libvalent_public_sources += files(libvalent_clipboard_public_sources)
libvalent_public_headers += files(libvalent_clipboard_public_headers)
libvalent_private_headers += files(libvalent_clipboard_private_headers)
libvalent_generated_headers += libvalent_clipboard_generated_headers
libvalent_generated_sources += libvalent_clipboard_generated_sources
libvalent_pkgconfig_subdirs += libvalent_clipboard_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-clipboard.h', '-DVALENT_CLIPBOARD_COMPILATION']

