# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_ui_header_dir = join_paths(libvalent_header_dir, 'ui')
libvalent_ui_header_subdir = join_paths(libvalent_header_subdir, 'ui')
libvalent_ui_generated_headers = []
libvalent_ui_generated_sources = []


# Headers
libvalent_ui_public_headers = [
  'valent-ui-utils.h',
]

libvalent_ui_private_headers = [
  'valent-application-credits.h',
  'valent-device-page.h',
  'valent-device-preferences-dialog.h',
  'valent-device-preferences-group.h',
  'valent-device-preferences-battery.h',
  'valent-device-preferences-clipboard.h',
  'valent-device-preferences-commands.h',
  'valent-device-preferences-commands-editor.h',
  'valent-device-preferences-connectivity.h',
  'valent-device-preferences-contacts.h',
  'valent-device-preferences-notification.h',
  'valent-device-preferences-sftp.h',
  'valent-device-preferences-share.h',
  'valent-device-preferences-telephony.h',
  'valent-input-remote.h',
  'valent-media-remote.h',
  'valent-menu-list.h',
  'valent-menu-stack.h',
  'valent-plugin-row.h',
  'valent-preferences-dialog.h',
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
  'valent-device-page.c',
  'valent-device-preferences-group.c',
  'valent-device-preferences-dialog.c',
  'valent-device-preferences-battery.c',
  'valent-device-preferences-clipboard.c',
  'valent-device-preferences-commands.c',
  'valent-device-preferences-commands-editor.c',
  'valent-device-preferences-connectivity.c',
  'valent-device-preferences-contacts.c',
  'valent-device-preferences-notification.c',
  'valent-device-preferences-sftp.c',
  'valent-device-preferences-share.c',
  'valent-device-preferences-telephony.c',
  'valent-input-remote.c',
  'valent-media-remote.c',
  'valent-menu-list.c',
  'valent-menu-stack.c',
  'valent-plugin-row.c',
  'valent-preferences-dialog.c',
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
  dependency('gtk4', version: gtk_version),
  dependency('libadwaita-1', version: libadwaita_version),
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_ui_public_sources)
libvalent_public_headers += files(libvalent_ui_public_headers)
libvalent_private_headers += files(libvalent_ui_private_headers)
libvalent_generated_headers += libvalent_ui_generated_headers
libvalent_generated_sources += libvalent_ui_generated_sources

