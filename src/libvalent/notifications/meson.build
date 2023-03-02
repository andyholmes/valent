# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_notifications_header_dir = join_paths(libvalent_header_dir, 'notifications')
libvalent_notifications_header_subdir = join_paths(libvalent_header_subdir, 'notifications')
libvalent_notifications_generated_headers = []
libvalent_notifications_generated_sources = []


# Headers
libvalent_notifications_public_headers = [
  'valent-notification.h',
  'valent-notifications.h',
  'valent-notifications-adapter.h',
]

libvalent_notifications_private_headers = [
]

install_headers(libvalent_notifications_public_headers,
  subdir: libvalent_notifications_header_subdir,
)


# Sources
libvalent_notifications_public_sources = [
  'valent-notification.c',
  'valent-notifications.c',
  'valent-notifications-adapter.c',
]


libvalent_include_directories += [include_directories('.')]
libvalent_public_sources += files(libvalent_notifications_public_sources)
libvalent_public_headers += files(libvalent_notifications_public_headers)
libvalent_private_headers += files(libvalent_notifications_private_headers)
libvalent_generated_headers += libvalent_notifications_generated_headers
libvalent_generated_sources += libvalent_notifications_generated_sources

