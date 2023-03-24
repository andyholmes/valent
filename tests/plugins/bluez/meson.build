# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

mock_bluez = find_program('mock_bluez.py')
installed_tests_wrappers += [mock_bluez]

# Dependencies
plugin_bluez_test_deps = [
  libvalent_test_dep,
  plugin_bluez_deps,
]

plugin_bluez_tests = {
  'test-bluez-plugin': mock_bluez,
}

foreach test, test_wrapper : plugin_bluez_tests
  plugin_bluez_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_bluez_test_deps,
    include_directories: plugin_bluez_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_bluez],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
  )

  test(test, test_wrapper,
            env: plugin_bluez_tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'bluez'],
  )

  installed_tests_plan += [{
    'program': test_program,
    'wrapper': test_wrapper,
  }]
endforeach

