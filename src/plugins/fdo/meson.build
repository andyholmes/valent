# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Require gdk-pixbuf for notifications
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.0', required: false)

if not gdk_pixbuf_dep.found()
  error('GDK-Pixbuf required for Freedesktop plugin')
endif

# Dependencies
plugin_fdo_deps = [
  libvalent_dep,

  gdk_pixbuf_dep,
]

# Sources
plugin_fdo_sources = files([
  'fdo-plugin.c',
  'valent-fdo-notifications.c',
  'valent-fdo-session.c',
])

plugin_fdo_include_directories = [include_directories('.')]

# Resources
plugin_fdo_info = i18n.merge_file(
   input: 'fdo.plugin.desktop.in',
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
)

#plugins_link_args += ['-Wl,--undefined=valent_fdo_plugin_register_types']
plugins_static += [plugin_fdo]

