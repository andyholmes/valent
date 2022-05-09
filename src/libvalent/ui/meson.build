# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_ui_header_dir = join_paths(libvalent_header_dir, 'ui')
libvalent_ui_header_subdir = join_paths(libvalent_header_subdir, 'ui')
libvalent_include_directories += include_directories('.')
libvalent_ui_generated_headers = []
libvalent_ui_generated_sources = []


# Headers
libvalent_ui_public_headers = [
  'libvalent-ui.h',
  'valent-application.h',
  'valent-device-activity.h',
  'valent-device-gadget.h',
  'valent-device-preferences-page.h',
  'valent-preferences-page.h',
]

libvalent_ui_private_headers = [
  'valent-device-panel.h',
  'valent-device-preferences-window.h',
  'valent-menu-list.h',
  'valent-menu-stack.h',
  'valent-preferences-window.h',
  'valent-preferences-window-private.h',
  'valent-window.h',
]

install_headers(libvalent_ui_public_headers,
  subdir: libvalent_ui_header_subdir
)


# Sources
libvalent_ui_public_sources = [
  'valent-application.c',
  'valent-device-activity.c',
  'valent-device-gadget.c',
  'valent-device-panel.c',
  'valent-device-preferences-page.c',
  'valent-device-preferences-window.c',
  'valent-menu-list.c',
  'valent-menu-stack.c',
  'valent-preferences-page.c',
  'valent-preferences-window.c',
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
libvalent_ui_deps = [
  libvalent_core_dep,
  libvalent_clipboard_dep,
  libvalent_contacts_dep,
  libvalent_input_dep,
  libvalent_media_dep,
  libvalent_mixer_dep,
  libvalent_notifications_dep,
  libvalent_session_dep,

  gtk_dep,
  libadwaita_dep,
]


# Library Definitions
libvalent_ui = static_library('valent-ui-' + valent_api_version,
                              libvalent_ui_public_sources,
                              libvalent_ui_generated_sources,
                              libvalent_ui_generated_headers,
  dependencies: libvalent_ui_deps,
        c_args: libvalent_c_args + release_args + ['-DVALENT_UI_COMPILATION'],
           pic: true,
)

libvalent_ui_dep = declare_dependency(
              sources: [libvalent_ui_generated_headers, libvalent_ui_private_headers],
         dependencies: libvalent_ui_deps,
            link_with: libvalent_ui,
  include_directories: include_directories('.'),
)


libvalent_static += [libvalent_ui]
libvalent_deps += [libvalent_ui_dep]
libvalent_public_sources += files(libvalent_ui_public_sources)
libvalent_public_headers += files(libvalent_ui_public_headers)
libvalent_private_headers += files(libvalent_ui_private_headers)
libvalent_generated_headers += libvalent_ui_generated_headers
libvalent_generated_sources += libvalent_ui_generated_sources
libvalent_include_subdirs += libvalent_ui_header_subdir
libvalent_gir_extra_args += ['--c-include=libvalent-ui.h', '-DVALENT_UI_COMPILATION']

