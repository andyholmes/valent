# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_contacts_header_dir = join_paths(libvalent_header_dir, 'contacts')
libvalent_contacts_header_subdir = join_paths(libvalent_header_subdir, 'contacts')
libvalent_contacts_generated_headers = []
libvalent_contacts_generated_sources = []


# Headers
libvalent_contacts_public_headers = [
  'valent-contacts.h',
  'valent-contacts-adapter.h',
  'valent-eds.h',
]

libvalent_contacts_private_headers = [
  'valent-contact-list.h',
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
  'valent-contact-list.c',
]


# GResources
libvalent_contacts_resources = gnome.compile_resources('libvalent-contacts-resources',
                                                       'libvalent-contacts.gresource.xml',
  c_name: 'valent_contacts',
)
libvalent_contacts_generated_sources += libvalent_contacts_resources
libvalent_contacts_generated_headers += [libvalent_contacts_resources[1]]


# Dependencies
libvalent_deps += [
  libebook_dep,
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_contacts_public_sources)
libvalent_public_headers += files(libvalent_contacts_public_headers)
libvalent_private_headers += files(libvalent_contacts_private_headers)
libvalent_generated_headers += libvalent_contacts_generated_headers
libvalent_generated_sources += libvalent_contacts_generated_sources

