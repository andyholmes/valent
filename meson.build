# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

project('valent', 'c',
          license: 'GPL-3.0-or-later',
          version: '1.0.0.alpha',
    meson_version: '>= 0.59.0',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=2',
    'werror=true',
  ],
)

gnome = import('gnome')
i18n = import('i18n')
pkgconfig = import('pkgconfig')


#
# Versioning
#
version_split = meson.project_version().split('.')
MAJOR_VERSION = version_split[0].to_int()
MINOR_VERSION = version_split[1].to_int()
MICRO_VERSION = version_split[2].to_int()

valent_version = '@0@.@1@.@2@'.format(MAJOR_VERSION, MINOR_VERSION, MICRO_VERSION)


#
# Build Profile
#
if get_option('profile') == 'devel'
  application_id = 'ca.andyholmes.Valent.Devel'
else
  application_id = 'ca.andyholmes.Valent'
endif


#
# Paths
#
prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
includedir = join_paths(prefix, get_option('includedir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
localedir = join_paths(prefix, get_option('localedir'))

docdir = join_paths(datadir, 'doc')
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')
girdir = join_paths(datadir, 'gir-1.0')
typelibdir = join_paths(libdir, 'girepository-1.0')
vapidir = join_paths(datadir, 'vala', 'vapi')

pkgdatadir = join_paths(datadir, meson.project_name())
pkgincludedir = join_paths(includedir, meson.project_name())
pkglibdir = join_paths(libdir, meson.project_name())
pkglibexecdir = join_paths(libexecdir, meson.project_name())
pkgpluginsdir = join_paths(pkglibdir, 'plugins')

po_dir = join_paths(meson.current_source_dir(), 'po')


#
# Compiler
#
cc = meson.get_compiler('c')

release_args = []
project_c_args = [
  '-Wfloat-equal',
  '-Wformat=2',
  '-Wincompatible-pointer-types',
  '-Wint-conversion',
  '-Wint-to-pointer-cast',
  '-Wmissing-include-dirs',
  '-Woverflow',
  '-Wpointer-arith',
  '-Wpointer-to-int-cast',
  '-Wredundant-decls',
  '-Wshadow',
  '-Wstrict-prototypes',
  '-Wundef',

  '-Wno-discarded-array-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',

  # These should be kept in sync (eg. GTK 4.10 requires GLib 2.72)
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_76',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_76',
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_10',
]
project_link_args = [
  '-Wl,-z,relro',
  '-Wl,-z,now',
]

if get_option('buildtype') != 'plain'
  project_c_args += ['-fstack-protector-strong']
endif

if get_option('debug')
  project_c_args += ['-fno-omit-frame-pointer']
endif

if get_option('optimization') in ['2', '3', 's']
  project_c_args += ['-DG_DISABLE_CAST_CHECKS']
  project_link_args += ['-Wl,-Bsymbolic']

  if not get_option('tests')
    release_args += ['-DG_DISABLE_ASSERT']
  endif
endif

if get_option('profile') == 'devel'
  project_c_args += ['-Wno-error=deprecated-declarations']
endif

add_project_arguments(cc.get_supported_arguments(project_c_args),
  language: 'c',
)

add_project_link_arguments(cc.get_supported_link_arguments(project_link_args),
  language: 'c',
)


#
# config.h
#
config_h = configuration_data()
config_h_inc = include_directories('.')
config_h_defines = {
  # Desktop
  'APPLICATION_ID':     application_id,

  # AC_INIT
  'PACKAGE_NAME':       meson.project_name(),
  'PACKAGE_TARNAME':    '@0@-@1@'.format(meson.project_name(), meson.project_version()),
  'PACKAGE_VERSION':    meson.project_version(),
  'PACKAGE_STRING':     '@0@ @1@'.format(meson.project_name(), meson.project_version()),
  'PACKAGE_BUGREPORT':  'https://github.com/andyholmes/@0@/issues'.format(meson.project_name()),
  'PACKAGE_URL':        'https://github.com/andyholmes/@0@'.format(meson.project_name()),

  # i18n
  'GETTEXT_PACKAGE':    meson.project_name(),
  'LOCALEDIR':          localedir,
}

foreach define, value : config_h_defines
  config_h.set_quoted(define, value)
endforeach

config_h_functions = {
  'HAVE_CLOCK_GETTIME': 'clock_gettime',
  'HAVE_LOCALTIME_R':   'localtime_r',
  'HAVE_SCHED_GETCPU':  'sched_getcpu',
}

foreach define, function : config_h_functions
  config_h.set(define, cc.has_function(function))
endforeach

if cc.has_argument('-fvisibility=hidden')
  config_h.set('_VALENT_EXTERN', '__attribute__((visibility("default"))) extern')
endif

configure_file(
         output: 'config.h',
  configuration: config_h,
)


#
# Dependencies
#
glib_version = '>= 2.76.0'
gtk_version = '>= 4.10.0'
gnutls_version = '>= 3.1.3'
json_glib_version = '>= 1.6.0'
libpeas_version = '>= 2.0.0'
eds_version = '>= 3.34'
sqlite_version = '>= 3.24.0'
libadwaita_version = '>= 1.5.alpha'
libportal_version = ['>= 0.6', '<= 0.7.1']

libm_dep = cc.find_library('m', required: true)
gio_dep = dependency('gio-2.0', version: glib_version)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_version)
gnutls_dep = dependency('gnutls', version: gnutls_version)
json_glib_dep = dependency('json-glib-1.0', version: json_glib_version)
libpeas_dep = dependency('libpeas-2', version: libpeas_version)
sqlite_dep = dependency('sqlite3', version: sqlite_version)

libportal_dep = dependency('libportal-gtk4',
   version: libportal_version,
  required: false,
)

if not libportal_dep.found()
  libportal_dep = dependency('libportal',
            version: libportal_version,
           fallback: ['libportal', 'libportal_dep'],
    default_options: [
      'docs=false',
      'introspection=false',
      'vapi=false',
      'werror=false',
    ],
  )
endif

# libvalent-contacts
libebook_dep = dependency('libebook-1.2', version: eds_version)

# libvalent-ui
gtk_dep = dependency('gtk4', version: gtk_version)
libadwaita_dep = dependency('libadwaita-1', version: libadwaita_version)

# Debugging & Testing
libsysprof_capture = dependency('sysprof-capture-4',
  required: false,
    static: true,
   version: '>= 3.38',
)
libwalbottle_dep = dependency('libwalbottle-0',
  required: false,
   version: '>= 0.3.0'
)


#
# Build
#
subdir('data')
subdir('src')
subdir('po')

if get_option('documentation')
  subdir('doc/sdk')
endif

if get_option('tests')
  subdir('tests')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
  update_desktop_database: true,
)


#
# Build Summary
#
summary({
  'Documentation': get_option('documentation'),
  'Introspection': get_option('introspection'),
  'VAPI':          get_option('vapi'),
  'Optimization':  get_option('optimization'),
  'Plugins':       get_option('plugins'),
  'Tests':         get_option('tests'),
  'Tracing':       get_option('tracing'),
  'Debugging':     get_option('debug'),
}, section: 'Build')

