# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_presenter_deps = [
  libvalent_dep,
]

# Sources
plugin_presenter_sources = files([
  'presenter-plugin.c',
  'valent-presenter-plugin.c',
  'valent-presenter-remote.c',
])

plugin_presenter_include_directories = [include_directories('.')]

# Resources
plugin_presenter_info = i18n.merge_file(
    args: plugins_po_args,
   input: 'presenter.plugin.desktop.in',
  output: 'presenter.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_presenter_resources = gnome.compile_resources('presenter-resources',
                                                     'presenter.gresource.xml',
        c_name: 'presenter',
  dependencies: [plugin_presenter_info],
)
plugin_presenter_sources += plugin_presenter_resources

# Static Build
plugin_presenter = static_library('plugin-presenter',
                                  plugin_presenter_sources,
    include_directories: plugin_presenter_include_directories,
           dependencies: plugin_presenter_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_presenter]

