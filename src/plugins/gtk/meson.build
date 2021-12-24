# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_gtk_deps = [
  libvalent_core_dep,
  libvalent_clipboard_dep,
  libvalent_notifications_dep,

  gtk_dep,
]

# Sources
plugin_gtk_sources = files([
  'gtk-plugin.c',
  'valent-gdk-clipboard.c',
  'valent-gtk-notifications.c',
])

plugin_gtk_include_directories = [include_directories('.')]

# Resources
plugin_gtk_info = i18n.merge_file('gtk.plugin',
   input: 'gtk.plugin.desktop.in',
  output: 'gtk.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_gtk_resources = gnome.compile_resources(
  'gtk-resources',
  'gtk.gresource.xml',
        c_name: 'gtk',
  dependencies: [plugin_gtk_info],
)
plugin_gtk_sources += plugin_gtk_resources

# Static Build
plugin_gtk = static_library('plugin-gtk',
                            plugin_gtk_sources,
  include_directories: plugin_gtk_include_directories,
         dependencies: plugin_gtk_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_gtk_plugin_register_types']
plugins_static += [plugin_gtk]

