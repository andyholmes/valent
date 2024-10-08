# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_battery_deps = [
  libvalent_dep,
]

# Sources
plugin_battery_sources = files([
  'battery-plugin.c',
  'valent-battery.c',
  'valent-battery-plugin.c',
])

plugin_battery_include_directories = [include_directories('.')]

# Resources
plugin_battery_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('battery.plugin.desktop.in'),
           output: 'battery.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'battery.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_battery_resources = gnome.compile_resources('battery-resources',
                                                   'battery.gresource.xml',
        c_name: 'battery',
  dependencies: [plugin_battery_info],
)
plugin_battery_sources += plugin_battery_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.battery.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_battery = static_library('plugin-battery',
                                plugin_battery_sources,
    include_directories: plugin_battery_include_directories,
           dependencies: plugin_battery_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_battery]

