# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

project('valent', 'c',
          license: 'GPL-3.0-or-later',
          version: '0.1.0',
    meson_version: '>= 0.53.0',
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
valent_version = meson.project_version()
version_split = valent_version.split('.')
MAJOR_VERSION = version_split[0].to_int()
MINOR_VERSION = version_split[1].to_int()
MICRO_VERSION = version_split[2].to_int()

valent_api_version = '@0@'.format(MAJOR_VERSION)
valent_api_name = 'valent-@0@'.format(valent_api_version)

#
# Build Profile
#
profile = get_option('profile')

if profile == 'devel'
  application_id = 'ca.andyholmes.Valent.Devel'
  application_path = '/ca/andyholmes/Valent/Devel'
  profile_suffix = ' (Devel)'
else
  application_id = 'ca.andyholmes.Valent'
  application_path = '/ca/andyholmes/Valent'
  profile_suffix = ''
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

pkgdatadir = join_paths(datadir, meson.project_name())
pkgincludedir = join_paths(includedir, meson.project_name())
pkglibdir = join_paths(libdir, meson.project_name())
pkglibexecdir = join_paths(libexecdir, meson.project_name())
pkgpluginsdir = join_paths(pkglibdir, 'plugins')

po_dir = join_paths(meson.source_root(), 'po')


#
# Compiler
#
cc = meson.get_compiler('c')


#
# config.h
#
config_h = configuration_data()
config_h_inc = include_directories('.')

config_h_defines = [
  # Desktop
  ['APPLICATION_ID', application_id],
  ['APPLICATION_PATH', application_path],

  # AC_INIT
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_API', valent_api_version],
  ['PACKAGE_BUGREPORT', 'https://github.com/andyholmes/' + meson.project_name() + '/issues'],
  ['PACKAGE_NAME', meson.project_name()],
  ['PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), valent_version)],
  ['PACKAGE_URL', 'https://github.com/andyholmes/' + meson.project_name()],
  ['PACKAGE_VERSION', meson.project_version()],

  # Paths
  ['PACKAGE_DATADIR', pkgdatadir],
  ['PACKAGE_LIBDIR', pkglibdir],
  ['PACKAGE_LIBEXECDIR', pkglibexecdir],
  ['PACKAGE_INCLUDEDIR', pkgincludedir],
  ['PACKAGE_PLUGINSDIR', pkgpluginsdir],

  ['PROFILE_NAME', profile],
  ['PROFILE_SUFFIX', profile_suffix],

  # i18n
  ['GETTEXT_PACKAGE', meson.project_name()],
  ['LOCALEDIR', localedir],
]

foreach define : config_h_defines
  config_h.set_quoted(define[0], define[1])
endforeach

config_h_functions = [
  'clock_gettime',
  'localtime_r',
]

foreach function : config_h_functions
  if cc.has_function(function)
    define = 'HAVE_' + function.underscorify().to_upper()
    config_h.set(define, 1)
  endif
endforeach


configure_file(
         output: 'config.h',
  configuration: config_h,
)

add_project_arguments([
  '-DHAVE_CONFIG_H',
  '-DVALENT_COMPILATION',
], language: 'c')


#
# Dependencies
#
gio_version = '>= 2.66.0'
gtk_version = '>= 4.0'
gnutls_version = '>= 3.1.3'
json_glib_version = '>= 1.6.0'
libpeas_version = '>= 1.22.0'
eds_version = '>= 3.34'
sqlite_version = '>= 3.24.0'
libadwaita_version = '>= 1.0.0.alpha.4'

libm_dep = cc.find_library('m', required: true)
gio_dep = dependency('gio-2.0', version: gio_version)
gio_unix_dep = dependency('gio-unix-2.0', version: gio_version)
gnutls_dep = dependency('gnutls', version: gnutls_version)
jsonglib_dep = dependency('json-glib-1.0', version: json_glib_version)
libpeas_dep = dependency('libpeas-1.0', version: libpeas_version)
gtk_dep = dependency('gtk4', version: gtk_version)
sqlite_dep = dependency('sqlite3', version: sqlite_version)

# libvalent-contacts
libebook_dep = dependency('libebook-1.2', version: eds_version)
libebook_contacts_dep = dependency('libebook-contacts-1.2', version: eds_version)
libedata_book_dep = dependency('libedata-book-1.2', version: eds_version)
libedataserver_dep = dependency('libedataserver-1.2', version: eds_version)

# libvalent-ui
libadwaita_dep = dependency('libadwaita-1', version: libadwaita_version, required: false)

if get_option('profiling')
  libsysprof_capture = dependency('sysprof-capture-4',
     version: '>= 3.38',
      static: true,
    required: true,
  )
endif


#
# Build Type
#
release_args = []
project_c_args = [
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

  '-Wno-error=cast-function-type',
  # TODO: These should be fixed instead of downgraded to warnings
  '-Wno-error=shadow',
  '-Wno-error=strict-aliasing',
  '-Wno-error=unused-function',
  '-Wno-error=unused-but-set-variable',
  '-Wno-error=unused-variable',
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

add_project_arguments(cc.get_supported_arguments(project_c_args),
  language: 'c',
)

add_project_link_arguments(cc.get_supported_link_arguments(project_link_args),
  language: 'c',
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

meson.add_install_script('build-aux/meson/postinstall.py')


#
# Build Summary
#
summary({
  'Documentation':         get_option('documentation'),
  'Introspection':         get_option('introspection'),
  'Tests':                 get_option('tests'),
  'Optimization':          get_option('optimization'),
  'Profiling':             get_option('profiling'),
  'Debugging':             get_option('debug'),
}, section: 'Build')

summary({
  'Bluetooth':             get_option('plugin_bluez'),
  'LAN':                   get_option('plugin_lan'),
}, section: 'Networking')

summary({
  'Battery':               get_option('plugin_battery'),
  'Clipboard':             get_option('plugin_clipboard'),
  'Contacts':              get_option('plugin_contacts'),
  'Find My Phone':         get_option('plugin_findmyphone'),
  'Lock':                  get_option('plugin_lock'),
  'Mousepad':              get_option('plugin_mousepad'),
  'MPRIS':                 get_option('plugin_mpris'),
  'Notification':          get_option('plugin_notification'),
  'Photo':                 get_option('plugin_photo'),
  'Ping':                  get_option('plugin_ping'),
  'Presenter':             get_option('plugin_presenter'),
  'Run Command':           get_option('plugin_runcommand'),
  'SFTP':                  get_option('plugin_sftp'),
  'Share':                 get_option('plugin_share'),
  'SMS':                   get_option('plugin_sms'),
  'System Volume':         get_option('plugin_systemvolume'),
  'Telephony':             get_option('plugin_telephony'),
}, section: 'Device Plugins')

summary({
  'Evolution Data Server': get_option('plugin_eds'),
  'freedesktop.org':       get_option('plugin_fdo'),
  'GTK':                   get_option('plugin_gtk'),
  'PulseAudio':            get_option('plugin_pulseaudio'),
  'XDP':                   get_option('plugin_xdp'),
}, section: 'Integration')

