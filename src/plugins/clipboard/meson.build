# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_clipboard_deps = [
  libvalent_dep,
]

# Sources
plugin_clipboard_sources = files([
  'clipboard-plugin.c',
  'valent-clipboard-plugin.c',
])

plugin_clipboard_include_directories = [include_directories('.')]

# Resources
plugin_clipboard_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('clipboard.plugin.desktop.in'),
           output: 'clipboard.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'clipboard.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_clipboard_resources = gnome.compile_resources('clipboard-resources',
                                                     'clipboard.gresource.xml',
        c_name: 'clipboard',
  dependencies: [plugin_clipboard_info],
)
plugin_clipboard_sources += plugin_clipboard_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.clipboard.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_clipboard = static_library('plugin-clipboard',
                                  plugin_clipboard_sources,
    include_directories: plugin_clipboard_include_directories,
           dependencies: plugin_clipboard_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_clipboard]

