# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

mock_gtk_notifications = find_program('mock_gtk_notifications.py')
installed_tests_wrappers += [mock_gtk_notifications]

# Dependencies
plugin_gtk_test_deps = [
  libvalent_test_dep,
  plugin_gtk_deps,
]

plugin_gtk_tests = {
  'test-gtk-notifications': mock_gtk_notifications,
  'test-gdk-clipboard': disabler(),
}

foreach test, test_wrapper : plugin_gtk_tests
  plugin_gtk_tests_env = [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
    'G_TEST_LIBDIR=@0@'.format(get_option('tests_libdir')),
  ]
  foreach envvar : tests_env
    if not (test_wrapper.found() and envvar.startswith('LD_LIBRARY_PATH'))
      plugin_gtk_tests_env += envvar
    endif
  endforeach

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: tests_c_args,
           dependencies: plugin_gtk_test_deps,
    include_directories: plugin_gtk_include_directories,
              link_args: tests_link_args,
             link_whole: [libvalent_test, plugin_gtk],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  if not test_wrapper.found()
    test_wrapper = test_program
  endif

  test(test, test_wrapper,
            env: plugin_gtk_tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'gtk'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
    'wrapper': test_wrapper,
  }]
endforeach

