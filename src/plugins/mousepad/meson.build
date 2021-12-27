# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_mousepad_deps = [
  libvalent_core_dep,
  libvalent_input_dep,
  libvalent_ui_dep,
]

# Sources
plugin_mousepad_sources = files([
  'mousepad-plugin.c',
  'valent-mousepad-dialog.c',
  'valent-mousepad-plugin.c',
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
)

#plugins_link_args += ['-Wl,--undefined=valent_mousepad_plugin_register_types']
plugins_static += [plugin_mousepad]

