# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_notification_deps = [
  libvalent_core_dep,
  libvalent_notifications_dep,
  libvalent_session_dep,
  libvalent_ui_dep,
]

# Sources
plugin_notification_sources = files([
  'notification-plugin.c',
  'valent-notification-plugin.c',
  'valent-notification-preferences.c',
  'valent-notification-reply-dialog.c',
])

plugin_notification_include_directories = [include_directories('.')]

# Resources
plugin_notification_info = i18n.merge_file('notification.plugin',
   input: 'notification.plugin.desktop.in',
  output: 'notification.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_notification_resources = gnome.compile_resources(
  'notification-resources',
  'notification.gresource.xml',
        c_name: 'notification',
  dependencies: [plugin_notification_info],
)
plugin_notification_sources += plugin_notification_resources

# Settings
install_data(
  ['ca.andyholmes.valent.notification.gschema.xml'],
  install_dir: schemadir
)

# Static Build
plugin_notification = static_library('plugin-notification',
                                     plugin_notification_sources,
  include_directories: plugin_notification_include_directories,
         dependencies: plugin_notification_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_notification_plugin_register_types']
plugins_static += [plugin_notification]

