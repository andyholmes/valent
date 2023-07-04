# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_sftp_deps = [
  libvalent_dep,
]

# Sources
plugin_sftp_sources = files([
  'sftp-plugin.c',
  'valent-sftp-plugin.c',
  'valent-sftp-preferences.c',
])

plugin_sftp_include_directories = [include_directories('.')]

# Resources
plugin_sftp_info = i18n.merge_file(
    args: plugins_po_args,
   input: 'sftp.plugin.desktop.in',
  output: 'sftp.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_sftp_resources = gnome.compile_resources('sftp-resources',
                                                'sftp.gresource.xml',
        c_name: 'sftp',
  dependencies: [plugin_sftp_info],
)
plugin_sftp_sources += plugin_sftp_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.sftp.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_sftp = static_library('plugin-sftp',
                             plugin_sftp_sources,
    include_directories: plugin_sftp_include_directories,
           dependencies: plugin_sftp_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_sftp]

