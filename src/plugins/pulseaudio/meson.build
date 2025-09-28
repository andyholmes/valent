# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libgvc = subproject('gvc',
  default_options: [
    'warning_level=0',
    'static=true',
    'alsa=false',
  ]
)
libgvc_dep = libgvc.get_variable('libgvc_dep')

if not libgvc_dep.found()
  error('"@0@" plugin requires libgnome-volume-control'.format(plugin_name))
endif

plugin_pulseaudio_deps = [
  libvalent_dep,
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
plugin_pulseaudio_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('pulseaudio.plugin.desktop.in'),
           output: 'pulseaudio.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'pulseaudio.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_pulseaudio_resources = gnome.compile_resources('pulseaudio-resources',
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
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_pulseaudio]

