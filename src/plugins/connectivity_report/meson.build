# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_connectivity_report_deps = [
  libvalent_dep,
]

# Sources
plugin_connectivity_report_sources = files([
  'connectivity_report-plugin.c',
  'valent-connectivity_report-plugin.c',
  'valent-telephony.c',
])

plugin_connectivity_report_include_directories = [include_directories('.')]

# Resources
plugin_connectivity_report_info = i18n.merge_file(
    args: plugins_po_args,
   input: 'connectivity_report.plugin.desktop.in',
  output: 'connectivity_report.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_connectivity_report_resources = gnome.compile_resources('connectivity_report-resources',
                                                               'connectivity_report.gresource.xml',
        c_name: 'connectivity_report',
  dependencies: [plugin_connectivity_report_info],
)
plugin_connectivity_report_sources += plugin_connectivity_report_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.connectivity_report.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_connectivity_report = static_library('plugin-connectivity_report',
                                            plugin_connectivity_report_sources,
    include_directories: plugin_connectivity_report_include_directories,
           dependencies: plugin_connectivity_report_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_connectivity_report]


