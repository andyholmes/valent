# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_bluez_deps = [
  libvalent_core_dep,
]

# Sources
plugin_bluez_sources = files([
  'bluez-plugin.c',
  'valent-bluez-channel-service.c',
  'valent-bluez-channel.c',
  'valent-bluez-device.c',
  'valent-bluez-profile.c',
  'valent-mux-io-stream.c',
  'valent-mux-connection.c',
  'valent-mux-input-stream.c',
  'valent-mux-output-stream.c',
])

plugin_bluez_include_directories = [include_directories('.')]

# Resources
plugin_bluez_info = i18n.merge_file(
   input: 'bluez.plugin.desktop.in',
  output: 'bluez.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_bluez_resources = gnome.compile_resources('bluez-resources',
                                                 'bluez.gresource.xml',
        c_name: 'bluez',
  dependencies: [plugin_bluez_info],
)
plugin_bluez_sources += plugin_bluez_resources

# Static Build
plugin_bluez = static_library('plugin-bluez',
                              plugin_bluez_sources,
  include_directories: plugin_bluez_include_directories,
         dependencies: plugin_bluez_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_bluez_plugin_register_types']
plugins_static += [plugin_bluez]

