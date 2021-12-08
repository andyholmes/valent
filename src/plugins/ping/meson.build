# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_ping_deps = [
  libvalent_core_dep,
]

# Sources
plugin_ping_sources = files([
  'ping-plugin.c',
  'valent-ping-plugin.c',
])

plugin_ping_include_directories = [include_directories('.')]

# Resources
plugin_ping_info = i18n.merge_file('ping.plugin',
   input: 'ping.plugin.desktop.in',
  output: 'ping.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_ping_resources = gnome.compile_resources(
  'ping-resources',
  'ping.gresource.xml',
        c_name: 'ping',
  dependencies: [plugin_ping_info],
)
plugin_ping_sources += plugin_ping_resources

# Static Build
plugin_ping = static_library('plugin-ping',
                             plugin_ping_sources,
  include_directories: plugin_ping_include_directories,
         dependencies: plugin_ping_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_ping_plugin_register_types']
plugins_static += [plugin_ping]

