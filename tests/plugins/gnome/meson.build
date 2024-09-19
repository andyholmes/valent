# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

mock_mutter = find_program('mock_mutter.py')
installed_tests_wrappers += [mock_mutter]
install_data(['remotedesktop.py', 'screencast.py'],
  install_dir: installed_tests_execdir,
)

# Dependencies
plugin_gnome_test_deps = [
  libvalent_test_dep,
  plugin_gnome_deps,
]

plugin_gnome_tests = {
  # FIXME: 'test-mutter-clipboard': mock_mutter,
  'test-mutter-input': mock_mutter,

  'test-contact-page': disabler(),
  'test-contact-row': disabler(),
  'test-conversation-page': disabler(),
  'test-conversation-row': disabler(),
  'test-date-label': disabler(),
  'test-device-page': disabler(),
  'test-device-preferences-dialog': disabler(),
  'test-gnome-application': disabler(),
  'test-input-remote': disabler(),
  'test-media-window': disabler(),
  'test-menu-stack': disabler(),
  'test-message-row': disabler(),
  'test-messages-window': disabler(),
  'test-notification-dialog': disabler(),
  'test-preferences-dialog': disabler(),
  'test-presenter-remote': disabler(),
  'test-share-dialog': disabler(),
  'test-window': disabler(),
}

foreach test, test_wrapper : plugin_gnome_tests
  plugin_gnome_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_gnome_test_deps,
    include_directories: plugin_gnome_include_directories,
              link_args: test_link_args,
             link_whole: [
               libvalent_test,
               plugin_gnome,
               plugin_battery,
               plugin_clipboard,
               plugin_runcommand,
               plugin_connectivity_report,
               plugin_contacts,
               plugin_notification,
               plugin_sftp,
               plugin_share,
               plugin_telephony,
             ],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  if not test_wrapper.found()
    test_wrapper = test_program
  endif

  test(test, test_wrapper,
            env: plugin_gnome_tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'gnome'],
  )

  installed_tests_plan += [{
    'program': test_program,
    'wrapper': test_wrapper,
  }]
endforeach

