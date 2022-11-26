# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_contacts_header_dir = join_paths(libvalent_header_dir, 'contacts')
libvalent_contacts_header_subdir = join_paths(libvalent_header_subdir, 'contacts')
libvalent_include_directories += include_directories('.')
libvalent_contacts_generated_headers = []
libvalent_contacts_generated_sources = []


# Headers
libvalent_contacts_public_headers = [
  'libvalent-contacts.h',
  'valent-contacts.h',
  'valent-contacts-adapter.h',
  'valent-contact-store.h',
  'valent-eds.h',
]

libvalent_contacts_private_headers = [
  'valent-contact-cache-private.h',
]

libvalent_contacts_enum_headers = [
]

install_headers(libvalent_contacts_public_headers,
  subdir: libvalent_contacts_header_subdir,
)


# Sources
libvalent_contacts_public_sources = [
  'valent-contacts.c',
  'valent-contacts-adapter.c',
  'valent-contact-cache.c',
  'valent-contact-store.c',
]


# Dependencies
libvalent_contacts_deps = [
  libvalent_core_dep,

  libebook_dep,
]


# Library Definitions
libvalent_contacts = static_library('valent-contacts-@0@'.format(libvalent_api_version),
                                    libvalent_contacts_public_sources,
                                    libvalent_contacts_generated_sources,
                                    libvalent_contacts_generated_headers,
        c_args: libvalent_c_args + release_args + ['-DVALENT_CONTACTS_COMPILATION'],
  dependencies: libvalent_contacts_deps,
)

libvalent_contacts_dep = declare_dependency(
              sources: [libvalent_contacts_generated_headers, libvalent_contacts_private_headers],
         dependencies: libvalent_contacts_deps,
            link_with: libvalent_contacts,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_contacts]
libvalent_deps += [libvalent_contacts_dep]
libvalent_public_sources += files(libvalent_contacts_public_sources)
libvalent_public_headers += files(libvalent_contacts_public_headers)
libvalent_private_headers += files(libvalent_contacts_private_headers)
libvalent_generated_headers += libvalent_contacts_generated_headers
libvalent_generated_sources += libvalent_contacts_generated_sources
libvalent_pkgconfig_subdirs += libvalent_contacts_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-contacts.h', '-DVALENT_CONTACTS_COMPILATION']

