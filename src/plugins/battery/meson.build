# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_battery_deps = [
  libvalent_core_dep,
  libvalent_device_dep,
  libvalent_ui_dep,
]

# Sources
plugin_battery_sources = files([
  'battery-plugin.c',
  'valent-battery.c',
  'valent-battery-gadget.c',
  'valent-battery-plugin.c',
  'valent-battery-preferences.c',
])

plugin_battery_include_directories = [include_directories('.')]

# Resources
plugin_battery_info = i18n.merge_file(
   input: 'battery.plugin.desktop.in',
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
install_data('ca.andyholmes.valent.battery.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_battery = static_library('plugin-battery',
                                 plugin_battery_sources,
  include_directories: plugin_battery_include_directories,
         dependencies: plugin_battery_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_battery_plugin_register_types']
plugins_static += [plugin_battery]

