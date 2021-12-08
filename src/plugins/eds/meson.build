# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_eds_deps = [
  libvalent_core_dep,
  libvalent_contacts_dep,

  libebook_dep,
  libebook_contacts_dep,
]

# Sources
plugin_eds_sources = files([
  'eds-plugin.c',
  'valent-ebook-provider.c',
  'valent-ebook-store.c',
  'valent-eds-utils.c',
])

plugin_eds_include_directories = include_directories('.')

# Resources
plugin_eds_info = i18n.merge_file('eds.plugin',
   input: 'eds.plugin.desktop.in',
  output: 'eds.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_eds_resources = gnome.compile_resources(
  'eds-resources',
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
)

#plugins_link_args += ['-Wl,--undefined=valent_eds_plugin_register_types']
plugins_static += [plugin_eds]

