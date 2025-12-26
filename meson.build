# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

project('valent', 'c', 'cpp',
          license: 'GPL-3.0-or-later',
          version: '1.0.0.alpha.50',
    meson_version: '>= 1.1',
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
  '-Wfloat-conversion',
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
  '-Wswitch-enum',
  '-Wundef',

  '-Wno-discarded-array-qualifiers',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',

  # These should be kept in sync with GTK and libadwaita
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_84',
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_84',

  # These are optional, but have minimum requirements if present
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_4_14',
  '-DADW_VERSION_MIN_REQUIRED=ADW_VERSION_1_6',

  '-Wno-error=analyzer-va-arg-type-mismatch',
]
project_link_args = [
  '-Wl,-z,relro', # Read-only segments after relocation
]

if get_option('buildtype') != 'plain'
  project_c_args += ['-fstack-protector-strong']
endif

if get_option('debug')
  project_c_args += ['-fno-omit-frame-pointer']
endif

if get_option('optimization') in ['2', '3', 's']
  project_c_args += [
    '-DG_DISABLE_CAST_CHECKS',
    '-floop-interchange',
  ]
  project_link_args += [
    '-Wl,-Bsymbolic', # Symbol visibility
    '-Wl,-z,now',     # Disable lazy binding (full RELRO)
    '-flto',          # Link-time optimization
    '-fno-plt',       # Avoid PLT for non-PIC code
  ]

  if not get_option('tests')
    release_args += ['-DG_DISABLE_ASSERT']
  endif
endif

if get_option('b_sanitize') == 'none'
  project_link_args += [
    '-Wl,-z,defs', # Prevent underlinking
  ]
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
# Dependencies
#
glib_version = '>= 2.84.0'
gtk_version = '>= 4.15.2'
glycin_version = '>= 2.0.1'
gnutls_version = '>= 3.1.3'
json_glib_version = '>= 1.6.0'
libpeas_version = '>= 2.0.0'
eds_version = '>= 3.48'
libadwaita_version = '>= 1.6'
libei_version = '>= 1.3.0'
libportal_version = ['>= 0.7.1']
tinysparql_version = '>= 3.0'
xkbcommon_version = '>= 1.0.0'

libm_dep = cc.find_library('m', required: true)
gio_dep = dependency('gio-2.0', version: glib_version)
gio_unix_dep = dependency('gio-unix-2.0', version: glib_version)
gnutls_dep = dependency('gnutls', version: gnutls_version)
json_glib_dep = dependency('json-glib-1.0', version: json_glib_version)
libpeas_dep = dependency('libpeas-2', version: libpeas_version)
libportal_dep = dependency('libportal', version: libportal_version)
tinysparql_dep = dependency('tracker-sparql-3.0', version: tinysparql_version)

# libvalent-contacts
libebook_dep = dependency('libebook-1.2', version: eds_version)
libphonenumber_dep = cc.find_library('phonenumber', required: true)

# Optional (depending on plugin)
gtk_dep = dependency('gtk4',
   version: gtk_version,
  required: false,
)
libadwaita_dep = dependency('libadwaita-1',
   version: libadwaita_version,
  required: false,
)
glycin_dep = dependency('glycin-2',
   version: glycin_version,
  required: false,
)
libportal_gtk4_dep = dependency('libportal-gtk4',
   version: libportal_version,
  required: false,
)
libei_dep = dependency('libei-1.0',
   version: libei_version,
  required: false,
)
xkbcommon_dep = dependency('xkbcommon',
   version: xkbcommon_version,
  required: false,
)

# Debugging & Testing
libsysprof_capture_dep = dependency('sysprof-capture-4',
  required: false,
    static: true,
   version: '>= 3.38',
)
libwalbottle_dep = dependency('libwalbottle-0',
  required: false,
   version: '>= 0.3.0'
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

config_h_dependencies = {
  'HAVE_GTK4':           gtk_dep,
  'HAVE_ADWAITA':        libadwaita_dep,
  'HAVE_GLYCIN':         glycin_dep,
  'HAVE_LIBPORTAL_GTK4': libportal_gtk4_dep,
  'HAVE_LIBEI':          libei_dep,
  'HAVE_SYSPROF':        libsysprof_capture_dep,
  'HAVE_WALBOTTLE':      libwalbottle_dep,
  'HAVE_XKBCOMMON':      xkbcommon_dep,
}

foreach define, dep : config_h_dependencies
  config_h.set(define, dep.found())
endforeach

config_h_functions = {
  'HAVE_CLOCK_GETTIME':       'clock_gettime',
  'HAVE_LOCALTIME_R':         'localtime_r',
  'HAVE_SCHED_GETCPU':        'sched_getcpu',
  'HAVE_REMOTE_DESKTOP_FULL': 'xdp_portal_create_remote_desktop_session_full',
}

foreach define, function : config_h_functions
  config_h.set(define, cc.has_function(function, dependencies: libportal_dep))
endforeach

if cc.has_argument('-fvisibility=hidden')
  config_h.set('_VALENT_EXTERN', '__attribute__((visibility("default"))) extern')
endif

configure_file(
         output: 'config.h',
  configuration: config_h,
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

