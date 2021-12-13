# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# libgnome-volume-control (PulseAudio)
enable_alsa = host_machine.system().contains('linux')

libgvc = subproject('gvc',
  default_options: [
    'warning_level=0',
    'static=true',
    'alsa=' + enable_alsa.to_string()
  ]
)
libgvc_dep = libgvc.get_variable('libgvc_dep')

if not libgvc_dep.found()
  error('libgnome-volume-control required for PulseAudio plugin')
endif

# Dependencies
plugin_pulseaudio_deps = [
  libvalent_core_dep,
  libvalent_mixer_dep,

  libgvc_dep,
]

# Sources
plugin_pulseaudio_sources = files([
  'pulseaudio-plugin.c',
  'valent-pa-mixer.c',
  'valent-pa-stream.c',
])

plugin_pulseaudio_include_directories = [include_directories('.')]

# Resources
plugin_pulseaudio_info = i18n.merge_file('pulseaudio.plugin',
   input: 'pulseaudio.plugin.desktop.in',
  output: 'pulseaudio.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_pulseaudio_resources = gnome.compile_resources(
  'pulseaudio-resources',
  'pulseaudio.gresource.xml',
        c_name: 'pulseaudio',
  dependencies: [plugin_pulseaudio_info],
)
plugin_pulseaudio_sources += plugin_pulseaudio_resources

# Static Build
plugin_pulseaudio = static_library('plugin-pulseaudio',
                                   plugin_pulseaudio_sources,
  include_directories: plugin_pulseaudio_include_directories,
         dependencies: plugin_pulseaudio_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_pulseaudio_plugin_register_types']
plugins_static += [plugin_pulseaudio]

