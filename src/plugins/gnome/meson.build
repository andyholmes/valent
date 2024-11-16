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
  'valent-gnome-application.c',
  'valent-input-remote.c',
  'valent-media-remote.c',
  'valent-media-window.c',
  'valent-menu-list.c',
  'valent-menu-stack.c',
  'valent-message-row.c',
  'valent-messages-window.c',
  'valent-plugin-row.c',
  'valent-preferences-dialog.c',
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

