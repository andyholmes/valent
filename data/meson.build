
subdir('firewalld')
subdir('gsettings')
subdir('metainfo')


#
# Desktop File
#
desktop_conf = configuration_data()
desktop_conf.set('application_id', application_id)
desktop_file = i18n.merge_file('desktop',
  input: configure_file(
    input: files('ca.andyholmes.Valent.desktop.in.in'),
    output: 'ca.andyholmes.Valent.desktop.in',
    configuration: desktop_conf
  ),
  output: '@0@.desktop'.format(application_id),
  install: true,
  install_dir: join_paths(datadir, 'applications'),
  po_dir: po_dir,
  type: 'desktop'
)

desktop_utils = find_program('desktop-file-validate', required: false)

if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
       args: [desktop_file],
    depends: [desktop_file],
  )
endif


#
# Application Icon
#
scalable_icondir = join_paths('icons', 'hicolor', 'scalable', 'apps')
install_data(
  join_paths(scalable_icondir, '@0@.svg'.format(application_id)),
  install_dir: join_paths(datadir, scalable_icondir),
       rename: '@0@.svg'.format(application_id),
)

symbolic_icondir = join_paths('icons', 'hicolor', 'symbolic', 'apps')
install_data(
  join_paths(symbolic_icondir, 'ca.andyholmes.Valent-symbolic.svg'),
  install_dir: join_paths(datadir, symbolic_icondir),
       rename: '@0@-symbolic.svg'.format(application_id),
)


#
# DBus Service
#
dbus_conf = configuration_data()
dbus_conf.set('application_id', application_id)
dbus_conf.set('bindir', bindir)
dbus_file = configure_file(
          input: 'ca.andyholmes.Valent.service.in',
         output: '@0@.service'.format(application_id),
  configuration: dbus_conf,
        install: true,
    install_dir: join_paths(datadir, 'dbus-1', 'services'),
)

