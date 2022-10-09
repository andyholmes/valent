# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_systemvolume_deps = [
  libvalent_core_dep,
  libvalent_device_dep,
  libvalent_mixer_dep,
]

# Sources
plugin_systemvolume_sources = files([
  'systemvolume-plugin.c',
  'valent-systemvolume-plugin.c',
])

plugin_systemvolume_include_directories = [include_directories('.')]

# Resources
plugin_systemvolume_info = i18n.merge_file(
   input: 'systemvolume.plugin.desktop.in',
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
install_data('ca.andyholmes.valent.systemvolume.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_systemvolume = static_library('plugin-systemvolume',
                                     plugin_systemvolume_sources,
  include_directories: plugin_systemvolume_include_directories,
         dependencies: plugin_systemvolume_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_systemvolume_plugin_register_types']
plugins_static += [plugin_systemvolume]

