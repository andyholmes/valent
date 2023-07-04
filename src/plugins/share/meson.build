# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_share_deps = [
  libvalent_dep,
]

# Sources
plugin_share_sources = files([
  'share-plugin.c',
  'valent-share-download.c',
  'valent-share-plugin.c',
  'valent-share-preferences.c',
  'valent-share-target.c',
  'valent-share-target-chooser.c',
  'valent-share-text-dialog.c',
  'valent-share-upload.c',
])

plugin_share_include_directories = [include_directories('.')]

# Resources
plugin_share_info = i18n.merge_file(
    args: plugins_po_args,
   input: 'share.plugin.desktop.in',
  output: 'share.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_share_resources = gnome.compile_resources('share-resources',
                                                 'share.gresource.xml',
        c_name: 'share',
  dependencies: [plugin_share_info],
)
plugin_share_sources += plugin_share_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.share.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_share = static_library('plugin-share',
                              plugin_share_sources,
    include_directories: plugin_share_include_directories,
           dependencies: plugin_share_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_share]

