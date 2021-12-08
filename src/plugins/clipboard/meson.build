# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_clipboard_deps = [
  libvalent_core_dep,
  libvalent_clipboard_dep,
  libvalent_ui_dep,
]

# Sources
plugin_clipboard_sources = files([
  'clipboard-plugin.c',
  'valent-clipboard-plugin.c',
  'valent-clipboard-preferences.c',
])

plugin_clipboard_include_directories = [include_directories('.')]

# Resources
plugin_clipboard_info = i18n.merge_file('clipboard.plugin',
   input: 'clipboard.plugin.desktop.in',
  output: 'clipboard.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_clipboard_resources = gnome.compile_resources(
  'clipboard-resources',
  'clipboard.gresource.xml',
        c_name: 'clipboard',
  dependencies: [plugin_clipboard_info],
)
plugin_clipboard_sources += plugin_clipboard_resources

# Settings
install_data(
  ['ca.andyholmes.valent.clipboard.gschema.xml'],
  install_dir: schemadir
)

# Static Build
plugin_clipboard = static_library('plugin-clipboard',
                                  plugin_clipboard_sources,
  include_directories: plugin_clipboard_include_directories,
         dependencies: plugin_clipboard_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_clipboard_plugin_register_types']
plugins_static += [plugin_clipboard]

