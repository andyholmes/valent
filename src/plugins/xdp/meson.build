# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_xdp_deps = [
  libvalent_dep,

  gtk_dep,
  libportal_dep,
]

if dependency('libportal-gtk4', required: false).found()
  plugins_c_args += [ '-DHAVE_LIBPORTAL_GTK4' ]
endif

# Sources
plugin_xdp_sources = files([
  'xdp-plugin.c',
  'valent-xdp-background.c',
  'valent-xdp-input.c',
  'valent-xdp-session.c',
  'valent-xdp-utils.c',
])

plugin_xdp_include_directories = [include_directories('.')]

# Resources
plugin_xdp_info = i18n.merge_file(
   input: 'xdp.plugin.desktop.in',
  output: 'xdp.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_xdp_resources = gnome.compile_resources('xdp-resources',
                                               'xdp.gresource.xml',
        c_name: 'xdp',
  dependencies: [plugin_xdp_info],
)
plugin_xdp_sources += plugin_xdp_resources

# Static Build
plugin_xdp = static_library('plugin-xdp',
                            plugin_xdp_sources,
  include_directories: plugin_xdp_include_directories,
         dependencies: plugin_xdp_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_xdp_plugin_register_types']
plugins_static += [plugin_xdp]

