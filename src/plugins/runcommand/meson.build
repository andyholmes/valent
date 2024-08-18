# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_runcommand_deps = [
  libvalent_dep,
]

# Sources
plugin_runcommand_sources = files([
  'runcommand-plugin.c',
  'valent-runcommand-plugin.c',
  'valent-runcommand-utils.c',
])

plugin_runcommand_include_directories = [include_directories('.')]

# Resources
plugin_runcommand_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('runcommand.plugin.desktop.in'),
           output: 'runcommand.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'runcommand.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_runcommand_resources = gnome.compile_resources('runcommand-resources',
                                                      'runcommand.gresource.xml',
        c_name: 'runcommand',
  dependencies: [plugin_runcommand_info],
)
plugin_runcommand_sources += plugin_runcommand_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.runcommand.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_runcommand = static_library('plugin-runcommand',
                                   plugin_runcommand_sources,
    include_directories: plugin_runcommand_include_directories,
           dependencies: plugin_runcommand_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_runcommand]
