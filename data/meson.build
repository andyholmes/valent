# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

subdir('gsettings')
subdir('metainfo')


# Desktop Files
desktop_conf = configuration_data()
desktop_conf.set('application_id', application_id)
desktop_file = i18n.merge_file(
        input: configure_file(
                  input: files('ca.andyholmes.Valent.desktop.in.in'),
                 output: 'ca.andyholmes.Valent.desktop.in',
          configuration: desktop_conf,
        ),
       output: '@0@.desktop'.format(application_id),
      install: true,
  install_dir: join_paths(datadir, 'applications'),
       po_dir: po_dir,
         type: 'desktop',
)

autostart_conf = configuration_data()
autostart_conf.set('application_id', application_id)
autostart_conf.set('bindir', bindir)
autostart_file = i18n.merge_file(
        input: configure_file(
                  input: files('ca.andyholmes.Valent-autostart.desktop.in.in'),
                 output: 'ca.andyholmes.Valent-autostart.desktop.in',
          configuration: autostart_conf,
        ),
       output: '@0@-autostart.desktop'.format(application_id),
      install: true,
  install_dir: join_paths(get_option('sysconfdir'), 'xdg', 'autostart'),
       po_dir: po_dir,
         type: 'desktop',
)

desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('validate-desktop-files', desktop_utils,
       args: [desktop_file, autostart_file],
    depends: [desktop_file, autostart_file],
  )
endif


# Application Icon
scalable_icondir = join_paths('icons', 'hicolor', 'scalable', 'apps')
install_data(join_paths(scalable_icondir, '@0@.svg'.format(application_id)),
  install_dir: join_paths(datadir, scalable_icondir),
       rename: '@0@.svg'.format(application_id),
)

symbolic_icondir = join_paths('icons', 'hicolor', 'symbolic', 'apps')
install_data(join_paths(symbolic_icondir, 'ca.andyholmes.Valent-symbolic.svg'),
  install_dir: join_paths(datadir, symbolic_icondir),
       rename: '@0@-symbolic.svg'.format(application_id),
)


# DBus Service
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

