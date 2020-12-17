# The subdirs update these before libvalent is compiled
libvalent_c_args = ['-Wno-missing-declarations']
libvalent_gir_extra_args = ['--pkg-export=valent-1.0']

libvalent_header_subdir = join_paths(valent_api_name, 'libvalent')
libvalent_header_dir = join_paths(includedir, libvalent_header_subdir)
libvalent_include_directories = []
libvalent_public_sources = []
libvalent_public_headers = []
libvalent_private_sources = []
libvalent_private_headers = []
libvalent_generated_sources = []
libvalent_generated_headers = []
libvalent_include_subdirs = []
libvalent_static = []
libvalent_deps = []

subdir('core')
subdir('clipboard')
subdir('contacts')
subdir('input')
subdir('media')
subdir('mixer')
subdir('notifications')
subdir('power')
subdir('session')
subdir('ui')


#
# Shared library
#
libvalent = shared_library('valent',
              version: valent_api_version,
            soversion: '0',
  include_directories: [include_directories('.')],
         dependencies: libvalent_deps,
           link_whole: libvalent_static,
               c_args: libvalent_c_args + release_args,
              install: true,
          install_dir: libdir,
)

libvalent_dep = declare_dependency(
            link_with: libvalent,
  include_directories: [include_directories('.')],
         dependencies: libvalent_deps
)

# pkgconfig
pkgconfig.generate(
      subdirs: libvalent_include_subdirs,
      version: meson.project_version(),
         name: valent_api_name,
     filebase: valent_api_name,
  description: 'Contains the plugin container for Valent.',
  install_dir: join_paths(libdir, 'pkgconfig'),
     requires: [
       'gio-2.0',
       'gio-unix-2.0',
       'gtk-4.0',
       'json-glib-1.0',
       'libpeas-1.0',
       'libebook-contacts-1.2',
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
    'EBookContacts-1.2',
  ]

  libvalent_gir_sources = [
    libvalent_public_sources +
    libvalent_public_headers +
    libvalent_generated_headers +
    libvalent_generated_sources
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

