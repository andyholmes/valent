# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_lan_deps = [
  libvalent_dep,
]

# Sources
plugin_lan_sources = files([
  'lan-plugin.c',
  'valent-lan-channel-service.c',
  'valent-lan-channel.c',
  'valent-lan-utils.c',
])

plugin_lan_include_directories = [include_directories('.')]

# Resources
plugin_lan_info = i18n.merge_file(
   input: 'lan.plugin.desktop.in',
  output: 'lan.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_lan_resources = gnome.compile_resources('lan-resources',
                                               'lan.gresource.xml',
        c_name: 'lan',
  dependencies: [plugin_lan_info],
)
plugin_lan_sources += plugin_lan_resources

# Static Build
plugin_lan = static_library('plugin-lan',
                            plugin_lan_sources,
    include_directories: plugin_lan_include_directories,
           dependencies: plugin_lan_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_lan]

