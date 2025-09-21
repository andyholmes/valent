# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_fdo_deps = [
  libvalent_dep,
]

if glycin_dep.found()
  plugin_fdo_deps += glycin_dep
endif

# Sources
plugin_fdo_sources = files([
  'fdo-plugin.c',
  'valent-fdo-notifications.c',
  'valent-fdo-session.c',
])

plugin_fdo_include_directories = [include_directories('.')]

# Resources
plugin_fdo_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('fdo.plugin.desktop.in'),
           output: 'fdo.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'fdo.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_fdo_resources = gnome.compile_resources('fdo-resources',
                                               'fdo.gresource.xml',
        c_name: 'fdo',
  dependencies: [plugin_fdo_info],
)
plugin_fdo_sources += plugin_fdo_resources

# Static Build
plugin_fdo = static_library('plugin-fdo',
                            plugin_fdo_sources,
    include_directories: plugin_fdo_include_directories,
           dependencies: plugin_fdo_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_fdo]

