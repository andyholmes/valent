# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Require libportal
libportal_dep = dependency('libportal', version: '>= 0.4', required: false)

if not libportal_dep.found()
  error('libportal required for XDP plugin')
endif

# Dependencies
plugin_xdp_deps = [
  libvalent_core_dep,
  libvalent_input_dep,

  libportal_dep,
]

# Sources
plugin_xdp_sources = files([
  'xdp-plugin.c',
  'valent-xdp-input.c',
  'valent-xdp-utils.c',
])

plugin_xdp_include_directories = [include_directories('.')]

# Resources
plugin_xdp_info = i18n.merge_file('xdp.plugin',
   input: 'xdp.plugin.desktop.in',
  output: 'xdp.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_xdp_resources = gnome.compile_resources(
  'xdp-resources',
  'xdp.gresource.xml',
        c_name: 'xdp',
  dependencies: [plugin_xdp_info],
)
plugin_xdp_sources += plugin_xdp_resources

# Settings
install_data(
  ['ca.andyholmes.valent.xdp.gschema.xml'],
  install_dir: schemadir
)

# Static Build
plugin_xdp = static_library('plugin-xdp',
                            plugin_xdp_sources,
  include_directories: plugin_xdp_include_directories,
         dependencies: plugin_xdp_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_xdp_plugin_register_types']
plugins_static += [plugin_xdp]

