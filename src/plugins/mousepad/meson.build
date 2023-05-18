# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_mousepad_deps = [
  libvalent_dep,
]

# Sources
plugin_mousepad_sources = files([
  'mousepad-plugin.c',
  'valent-mousepad-plugin.c',
  'valent-mousepad-remote.c',
])

plugin_mousepad_include_directories = [include_directories('.')]

# Resources
plugin_mousepad_info = i18n.merge_file(
   input: 'mousepad.plugin.desktop.in',
  output: 'mousepad.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_mousepad_resources = gnome.compile_resources('mousepad-resources',
                                                    'mousepad.gresource.xml',
        c_name: 'mousepad',
  dependencies: [plugin_mousepad_info],
)
plugin_mousepad_sources += plugin_mousepad_resources

# Static Build
plugin_mousepad = static_library('plugin-mousepad',
                                 plugin_mousepad_sources,
    include_directories: plugin_mousepad_include_directories,
           dependencies: plugin_mousepad_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_mousepad]

