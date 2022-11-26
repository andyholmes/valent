# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_api_version = meson.project_version().split('.')[0]
libvalent_api_name = 'libvalent-@0@'.format(libvalent_api_version)
libvalent_so_version = valent_version

# The subdirs update these before libvalent is compiled
libvalent_c_args = []
libvalent_link_args = []
libvalent_gir_extra_args = ['--pkg-export=@0@'.format(libvalent_api_name)]

libvalent_header_subdir = join_paths(libvalent_api_name, 'libvalent')
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
subdir('input')
subdir('media')
subdir('mixer')
subdir('notifications')
subdir('session')
subdir('device')
subdir('ui')


#
# Shared library
#
libvalent = shared_library('valent-@0@'.format(libvalent_api_version),
       version: libvalent_api_version,
     soversion: libvalent_so_version,
        c_args: libvalent_c_args + release_args,
     link_args: libvalent_link_args,
    link_whole: libvalent_static,
  dependencies: libvalent_deps,
       install: true,
)

libvalent_dep = declare_dependency(
            link_with: libvalent,
  include_directories: libvalent_include_directories,
         dependencies: libvalent_deps,
)

# pkgconfig
pkgconfig.generate(
         name: 'Valent',
  description: 'Core implementation and plugin API for Valent.',
      version: libvalent_so_version,
     filebase: libvalent_api_name,
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
    'Gio-2.0',
    'Gtk-4.0',
    'Json-1.0',
    'Adw-1',
    'EBook-1.2',
    'Peas-1.0',
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
              nsversion: libvalent_api_version,
          symbol_prefix: meson.project_name(),
      identifier_prefix: 'Valent',
               includes: libvalent_gir_includes,
                install: true,
        install_dir_gir: girdir,
    install_dir_typelib: typelibdir,
             extra_args: libvalent_gir_extra_args,
  )
endif

