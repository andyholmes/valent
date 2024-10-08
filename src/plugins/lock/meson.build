# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_lock_deps = [
  libvalent_dep,
]

# Sources
plugin_lock_sources = files([
  'lock-plugin.c',
  'valent-lock-plugin.c',
])

plugin_lock_include_directories = [include_directories('.')]

# Resources
plugin_lock_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('lock.plugin.desktop.in'),
           output: 'lock.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'lock.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_lock_resources = gnome.compile_resources('lock-resources',
                                                'lock.gresource.xml',
        c_name: 'lock',
  dependencies: [plugin_lock_info],
)
plugin_lock_sources += plugin_lock_resources

# Static Build
plugin_lock = static_library('plugin-lock',
                             plugin_lock_sources,
    include_directories: plugin_lock_include_directories,
           dependencies: plugin_lock_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_lock]

