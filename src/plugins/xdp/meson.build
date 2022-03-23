# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Require libportal
libportal_version = '0.5'
libportal_dep = dependency('libportal-gtk4',
   version: libportal_version,
  required: false,
)

if libportal_dep.found()
  plugins_c_args += [ '-DHAVE_LIBPORTAL_GTK4' ]
else
  libportal_dep = dependency('libportal',
     version: libportal_version,
    required: false,
  )

  if not libportal_dep.found()
    libportal = subproject('libportal',
      default_options: [
        'backends=gtk4',
        'docs=false',
        'introspection=false',
        'vapi=false',
        'werror=false',
      ],
    )
    libportal_dep = libportal.get_variable('libportal_dep')
    plugins_c_args += [ '-DHAVE_LIBPORTAL_GTK4' ]
  endif
endif

# Dependencies
plugin_xdp_deps = [
  libvalent_core_dep,
  libvalent_input_dep,

  libportal_dep,
]

# Sources
plugin_xdp_sources = files([
  'xdp-plugin.c',
  'valent-xdp-input.c',
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

# Settings
install_data('ca.andyholmes.valent.xdp.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_xdp = static_library('plugin-xdp',
                            plugin_xdp_sources,
  include_directories: plugin_xdp_include_directories,
         dependencies: plugin_xdp_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_xdp_plugin_register_types']
plugins_static += [plugin_xdp]

