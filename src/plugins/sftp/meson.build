# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_sftp_deps = [
  libvalent_core_dep,
  libvalent_device_dep,
  libvalent_ui_dep,
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
install_data('ca.andyholmes.valent.sftp.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_sftp = static_library('plugin-sftp',
                             plugin_sftp_sources,
  include_directories: plugin_sftp_include_directories,
         dependencies: plugin_sftp_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_sftp_plugin_register_types']
plugins_static += [plugin_sftp]

