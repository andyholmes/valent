# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# pipewire (Pipewire)
libpipewire_dep = dependency('libpipewire-0.3', version: '>= 1.0.0')

# Dependencies
plugin_pipewire_deps = [
  libvalent_dep,

  libpipewire_dep,
]

# Sources
plugin_pipewire_sources = files([
  'pipewire-plugin.c',
  'valent-pipewire-mixer.c',
  'valent-pipewire-stream.c',
])

plugin_pipewire_include_directories = [include_directories('.')]

# Resources
plugin_pipewire_info = i18n.merge_file(
   input: 'pipewire.plugin.desktop.in',
  output: 'pipewire.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_pipewire_resources = gnome.compile_resources('pipewire-resources',
                                                    'pipewire.gresource.xml',
        c_name: 'pipewire',
  dependencies: [plugin_pipewire_info],
)
plugin_pipewire_sources += plugin_pipewire_resources

# Static Build
plugin_pipewire = static_library('plugin-pipewire',
                                 plugin_pipewire_sources,
  include_directories: plugin_pipewire_include_directories,
         dependencies: plugin_pipewire_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_pipewire_plugin_register_types']
plugins_static += [plugin_pipewire]

