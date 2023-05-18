# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_gnome_deps = [
  libvalent_dep,
]

# Sources
plugin_gnome_sources = files([
  'gnome-plugin.c',
  'valent-gnome-clipboard.c',
  'valent-mutter-input.c',
])

plugin_gnome_include_directories = [include_directories('.')]

# Resources
plugin_gnome_info = i18n.merge_file(
   input: 'gnome.plugin.desktop.in',
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

