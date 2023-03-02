# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Require GStreamer for ringing
gstreamer_dep = dependency('gstreamer-1.0', version: '>= 1.16', required: false)

if not gstreamer_dep.found()
  error('GStreamer required for FindMyPhone plugin')
endif

# Dependencies
plugin_findmyphone_deps = [
  libvalent_dep,

  gstreamer_dep
]

# Sources
plugin_findmyphone_sources = files([
  'findmyphone-plugin.c',
  'valent-findmyphone-plugin.c',
  'valent-findmyphone-ringer.c',
])

plugin_findmyphone_include_directories = [include_directories('.')]

# Resources
plugin_findmyphone_info = i18n.merge_file(
   input: 'findmyphone.plugin.desktop.in',
  output: 'findmyphone.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_findmyphone_resources = gnome.compile_resources('findmyphone-resources',
                                                       'findmyphone.gresource.xml',
        c_name: 'findmyphone',
  dependencies: [plugin_findmyphone_info],
)
plugin_findmyphone_sources += plugin_findmyphone_resources

# Static Build
plugin_findmyphone = static_library('plugin-findmyphone',
                                    plugin_findmyphone_sources,
  include_directories: plugin_findmyphone_include_directories,
         dependencies: plugin_findmyphone_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_findmyphone_plugin_register_types']
plugins_static += [plugin_findmyphone]

