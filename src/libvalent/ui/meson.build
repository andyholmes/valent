# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_ui_header_dir = join_paths(libvalent_header_dir, 'ui')
libvalent_ui_header_subdir = join_paths(libvalent_header_subdir, 'ui')
libvalent_ui_generated_headers = []
libvalent_ui_generated_sources = []


# Headers
libvalent_ui_public_headers = [
  'valent-device-gadget.h',
  'valent-device-preferences-group.h',
  'valent-preferences-page.h',
  'valent-ui-utils.h',
]

libvalent_ui_private_headers = [
  'valent-application-credits.h',
  'valent-device-page.h',
  'valent-device-preferences-window.h',
  'valent-media-remote.h',
  'valent-menu-list.h',
  'valent-menu-stack.h',
  'valent-preferences-window.h',
  'valent-ui-manager.h',
  'valent-ui-utils-private.h',
  'valent-window.h',
]

install_headers(libvalent_ui_public_headers,
  subdir: libvalent_ui_header_subdir,
)


# Sources
libvalent_ui_public_sources = [
  'libvalent-ui.c',
  'valent-device-gadget.c',
  'valent-device-page.c',
  'valent-device-preferences-group.c',
  'valent-device-preferences-window.c',
  'valent-media-remote.c',
  'valent-menu-list.c',
  'valent-menu-stack.c',
  'valent-preferences-page.c',
  'valent-preferences-window.c',
  'valent-time-helpers.c',
  'valent-ui-main.c',
  'valent-ui-manager.c',
  'valent-ui-utils.c',
  'valent-window.c',
]

libvalent_ui_sources = libvalent_ui_public_sources


# GResources
libvalent_ui_resources = gnome.compile_resources('libvalent-ui-resources',
                                                 'libvalent-ui.gresource.xml',
  c_name: 'valent_ui',
)
libvalent_ui_generated_sources += libvalent_ui_resources
libvalent_ui_generated_headers += [libvalent_ui_resources[1]]


# Dependencies
libvalent_deps += [
  gtk_dep,
  libadwaita_dep,
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_ui_public_sources)
libvalent_public_headers += files(libvalent_ui_public_headers)
libvalent_private_headers += files(libvalent_ui_private_headers)
libvalent_generated_headers += libvalent_ui_generated_headers
libvalent_generated_sources += libvalent_ui_generated_sources

