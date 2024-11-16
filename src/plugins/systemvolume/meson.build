# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_systemvolume_deps = [
  libvalent_dep,
]

# Sources
plugin_systemvolume_sources = files([
  'systemvolume-plugin.c',
  'valent-systemvolume-device.c',
  'valent-systemvolume-plugin.c',
])

plugin_systemvolume_include_directories = [include_directories('.')]

# Resources
plugin_systemvolume_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('systemvolume.plugin.desktop.in'),
           output: 'systemvolume.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'systemvolume.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_systemvolume_resources = gnome.compile_resources('systemvolume-resources',
                                                        'systemvolume.gresource.xml',
        c_name: 'systemvolume',
  dependencies: [plugin_systemvolume_info],
)
plugin_systemvolume_sources += plugin_systemvolume_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.systemvolume.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_systemvolume = static_library('plugin-systemvolume',
                                     plugin_systemvolume_sources,
    include_directories: plugin_systemvolume_include_directories,
           dependencies: plugin_systemvolume_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_systemvolume]

