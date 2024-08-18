# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_sms_deps = [
  libvalent_dep,
]

# Sources
plugin_sms_sources = files([
  'sms-plugin.c',
  'valent-sms-device.c',
  'valent-sms-plugin.c',
])

plugin_sms_include_directories = [include_directories('.')]

# Resources
plugin_sms_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('sms.plugin.desktop.in'),
           output: 'sms.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'sms.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_sms_resources = gnome.compile_resources('sms-resources',
                                               'sms.gresource.xml',
        c_name: 'sms',
  dependencies: [plugin_sms_info],
)
plugin_sms_sources += plugin_sms_resources

# Static Build
plugin_sms = static_library('plugin-sms',
                            plugin_sms_sources,
    include_directories: plugin_sms_include_directories,
           dependencies: plugin_sms_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_sms]

