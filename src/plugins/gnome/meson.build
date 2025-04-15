# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

if not (gtk_dep.found() and libadwaita_dep.found())
  error('GTK4 and Adwaita required for GNOME plugin')
endif

# Dependencies
plugin_gnome_deps = [
  libvalent_dep,
  gtk_dep,
  libadwaita_dep,
]

# Sources
plugin_gnome_sources = files([
  'gnome-plugin.c',
  'valent-mutter-clipboard.c',

  'valent-contact-page.c',
  'valent-contact-row.c',
  'valent-conversation-page.c',
  'valent-conversation-row.c',
  'valent-date-label.c',
  'valent-device-page.c',
  'valent-device-row.c',
  'valent-device-preferences-dialog.c',
  'valent-gnome-application.c',
  'valent-input-remote.c',
  'valent-media-remote.c',
  'valent-media-window.c',
  'valent-menu-list.c',
  'valent-menu-stack.c',
  'valent-message-row.c',
  'valent-messages-window.c',
  'valent-mixer-remote.c',
  'valent-plugin-row.c',
  'valent-preferences-command-editor.c',
  'valent-preferences-dialog.c',
  'valent-preferences-page.c',
  'valent-preferences-other-page.c',
  'valent-preferences-status-page.c',
  'valent-preferences-sync-page.c',
  'valent-presenter-remote.c',
  'valent-share-dialog.c',
  'valent-time-helpers.c',
  'valent-ui-main.c',
  'valent-ui-utils.c',
  'valent-window.c',
])

plugin_gnome_include_directories = [include_directories('.')]

# Resources
plugin_gnome_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('gnome.plugin.desktop.in'),
           output: 'gnome.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'gnome.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_gnome_resources = gnome.compile_resources('gnome-resources',
                                                 'gnome.gresource.xml',
        c_name: 'gnome',
  dependencies: [plugin_gnome_info],
    source_dir: ['data' / 'icons' ],
)
plugin_gnome_sources += plugin_gnome_resources

# Static Build
plugin_gnome = static_library('plugin-gnome',
                              plugin_gnome_sources,
    include_directories: plugin_gnome_include_directories,
           dependencies: plugin_gnome_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_gnome]

