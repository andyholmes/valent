# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Require libaperture for taking photos
gstreamer_video_dep = dependency('gstreamer-video-1.0', version: '>= 1.16', required: false)

if not gstreamer_video_dep.found()
  error('GStreamer Video required for Photo plugin')
endif

# Dependencies
plugin_photo_deps = [
  libvalent_core_dep,
  gstreamer_video_dep,
]

# Sources
plugin_photo_sources = files([
  'photo-plugin.c',
  'valent-photo-plugin.c',
])

plugin_photo_include_directories = [include_directories('.')]

# Resources
plugin_photo_info = i18n.merge_file('photo.plugin',
   input: 'photo.plugin.desktop.in',
  output: 'photo.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_photo_resources = gnome.compile_resources(
  'photo-resources',
  'photo.gresource.xml',
        c_name: 'photo',
  dependencies: [plugin_photo_info],
)
plugin_photo_sources += plugin_photo_resources

# Static Build
plugin_photo = static_library('plugin-photo',
                              plugin_photo_sources,
  include_directories: plugin_photo_include_directories,
         dependencies: plugin_photo_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_photo_plugin_register_types']
plugins_static += [plugin_photo]

