# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_notification_deps = [
  libvalent_dep,
]

if glycin_dep.found()
  plugin_notification_deps += glycin_dep
endif
if gtk_dep.found()
  plugin_notification_deps += gtk_dep
endif

# Sources
plugin_notification_sources = files([
  'notification-plugin.c',
  'valent-notification-plugin.c',
  'valent-notification-upload.c',
])

plugin_notification_include_directories = [include_directories('.')]

# Resources
plugin_notification_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('notification.plugin.desktop.in'),
           output: 'notification.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'notification.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_notification_resources = gnome.compile_resources('notification-resources',
                                                        'notification.gresource.xml',
        c_name: 'notification',
  dependencies: [plugin_notification_info],
)
plugin_notification_sources += plugin_notification_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.notification.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_notification = static_library('plugin-notification',
                                     plugin_notification_sources,
    include_directories: plugin_notification_include_directories,
           dependencies: plugin_notification_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_notification]

