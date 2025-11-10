# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

mock_fdo_notifications = find_program('mock_fdo_notifications.py')
mock_logind = find_program('mock_logind.py')
installed_tests_wrappers += [mock_fdo_notifications, mock_logind]

# Dependencies
plugin_fdo_test_deps = [
  libvalent_test_dep,
  plugin_fdo_deps,
]

plugin_fdo_tests = {
  'test-fdo-notifications': mock_fdo_notifications,
  'test-fdo-session': mock_logind,
}

foreach test, test_wrapper : plugin_fdo_tests
  plugin_fdo_tests_env = [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
    'G_TEST_LIBDIR=@0@'.format(get_option('tests_libdir')),
  ]
  foreach envvar : tests_env
    if not (test_wrapper.found() and envvar.startswith('LD_LIBRARY_PATH'))
      plugin_fdo_tests_env += envvar
    endif
  endforeach

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_fdo_test_deps,
    include_directories: plugin_fdo_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_fdo],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_wrapper,
            env: plugin_fdo_tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'fdo'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
    'wrapper': test_wrapper,
  }]
endforeach

