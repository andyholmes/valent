# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

libvalent_api_version = meson.project_version().split('.')[0]
libvalent_api_name = 'libvalent-@0@'.format(libvalent_api_version)

# The subdirs update these before libvalent is compiled
libvalent_c_args = ['-DVALENT_COMPILATION']
libvalent_link_args = []

libvalent_header_subdir = join_paths(libvalent_api_name, 'libvalent')
libvalent_header_dir = join_paths(includedir, libvalent_header_subdir)
libvalent_include_directories = [config_h_inc, include_directories('.')]
libvalent_public_sources = []
libvalent_public_headers = ['valent.h']
libvalent_private_sources = []
libvalent_private_headers = []
libvalent_generated_sources = []
libvalent_generated_headers = []
libvalent_static = []
libvalent_deps = []

subdir('core')
subdir('clipboard')
subdir('contacts')
subdir('input')
subdir('media')
subdir('messages')
subdir('mixer')
subdir('notifications')
subdir('session')
subdir('device')


#
# Shared library
#
install_headers('valent.h',
  subdir: libvalent_header_subdir,
)

libvalent = shared_library('valent-@0@'.format(libvalent_api_version),
                           libvalent_public_sources,
                           libvalent_generated_sources,
                           libvalent_generated_headers,
    include_directories: libvalent_include_directories,
           dependencies: libvalent_deps,
                 c_args: libvalent_c_args + release_args,
              link_args: libvalent_link_args,
  gnu_symbol_visibility: 'hidden',
                version: valent_version,
              soversion: libvalent_api_version,
                install: true,
)

libvalent_dep = declare_dependency(
              sources: [libvalent_generated_headers, libvalent_private_headers],
            link_with: libvalent,
  include_directories: libvalent_include_directories,
         dependencies: libvalent_deps,
)

# pkgconfig
pkgconfig.generate(
         name: 'Valent',
  description: 'Core implementation and plugin API for Valent.',
      version: valent_version,
     filebase: libvalent_api_name,
    libraries: [libvalent],
      subdirs: [libvalent_header_subdir],
     requires: [
       'gio-2.0',
       'json-glib-1.0',
       'libebook-1.2',
       'libpeas-2',
       'tracker-sparql-3.0',
     ],
     requires_private: [
       'gio-unix-2.0',
       'gnutls',
       'libportal',
     ],
)

# GObject Introspection
if get_option('introspection')
  libvalent_gir_extra_args = [
    '--c-include=valent.h',
    '--pkg-export=@0@'.format(libvalent_api_name),
  ]

  libvalent_gir_includes = [
    'Gio-2.0',
    'Json-1.0',
    'EBook-1.2',
    'Peas-2',
    'Tracker-3.0',
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

  # Vala API Bindings
  if get_option('vapi')
    libvalent_vapi = gnome.generate_vapi(libvalent_api_name,
           sources: libvalent_gir[0],
          packages: [
             'gio-2.0',
             'json-glib-1.0',
             'libebook-1.2',
             'libpeas-2',
             'tracker-sparql-3.0',
          ],
           install: true,
       install_dir: vapidir,
    )
  endif
endif

