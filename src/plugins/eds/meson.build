# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_eds_deps = [
  libvalent_dep,

  libebook_dep,
]

# Sources
plugin_eds_sources = files([
  'eds-plugin.c',
  'valent-ebook-adapter.c',
  'valent-ebook-store.c',
  'valent-eds-utils.c',
])

plugin_eds_include_directories = include_directories('.')

# Resources
plugin_eds_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('eds.plugin.desktop.in'),
           output: 'eds.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'eds.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_eds_resources = gnome.compile_resources('eds-resources',
                                               'eds.gresource.xml',
        c_name: 'eds',
  dependencies: [plugin_eds_info],
)
plugin_eds_sources += plugin_eds_resources

# Static Build
plugin_eds = static_library('plugin-eds',
                            plugin_eds_sources,
    include_directories: plugin_eds_include_directories,
           dependencies: plugin_eds_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_eds]

