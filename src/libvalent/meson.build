# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# The subdirs update these before libvalent is compiled
libvalent_c_args = []
libvalent_gir_extra_args = ['--pkg-export=@0@'.format(valent_api_name)]

libvalent_header_subdir = join_paths(valent_api_name, 'libvalent')
libvalent_header_dir = join_paths(includedir, libvalent_header_subdir)
libvalent_include_directories = []
libvalent_public_sources = []
libvalent_public_headers = []
libvalent_private_sources = []
libvalent_private_headers = []
libvalent_generated_sources = []
libvalent_generated_headers = []
libvalent_pkgconfig_subdirs = []
libvalent_static = []
libvalent_deps = []

subdir('core')
subdir('clipboard')
subdir('contacts')
subdir('device')
subdir('input')
subdir('media')
subdir('mixer')
subdir('notifications')
subdir('session')
subdir('ui')


#
# Shared library
#
libvalent = shared_library('valent',
              version: valent_api_version,
            soversion: '0',
               c_args: libvalent_c_args + release_args,
         dependencies: libvalent_deps,
           link_whole: libvalent_static,
  include_directories: libvalent_include_directories,
              install: true,
          install_dir: libdir,
)

libvalent_dep = declare_dependency(
         dependencies: libvalent_deps,
            link_with: libvalent,
  include_directories: libvalent_include_directories,
)

# pkgconfig
pkgconfig.generate(
         name: 'Valent',
  description: 'Core implementation and plugin API for Valent.',
      version: valent_version,
     filebase: valent_api_name,
    libraries: [libvalent],
      subdirs: libvalent_pkgconfig_subdirs,
     requires: [
       'gio-2.0',
       'gtk4',
       'json-glib-1.0',
       'libadwaita-1',
       'libebook-1.2',
       'libpeas-1.0',
     ],
     requires_private: [
       'gio-unix-2.0',
       'gnutls',
       'libportal',
     ],
)

# GObject Introspection
if get_option('introspection')
  libvalent_gir_includes = [
    'GObject-2.0',
    'Gio-2.0',
    'Gdk-4.0',
    'Gtk-4.0',
    'Json-1.0',
    'Peas-1.0',
    'EBook-1.2',
  ]

  libvalent_gir_sources = [
    libvalent_public_sources,
    libvalent_public_headers,
    libvalent_generated_headers,
    libvalent_generated_sources,
  ]

  libvalent_gir = gnome.generate_gir(libvalent,
                sources: libvalent_gir_sources,
              namespace: 'Valent',
              nsversion: valent_api_version,
          symbol_prefix: meson.project_name(),
      identifier_prefix: 'Valent',
               includes: libvalent_gir_includes,
                install: true,
        install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
             extra_args: libvalent_gir_extra_args,
  )
endif

