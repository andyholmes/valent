# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_bluez_test_deps = [
  libvalent_test_dep,
  plugin_bluez_deps,
]

plugin_bluez_tests = {
  'test-bluez-plugin': find_program('mock_bluez.py'),
}

foreach test, test_runner : plugin_bluez_tests
  plugin_bluez_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_bluez_test_deps,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_bluez],
    include_directories: plugin_bluez_include_directories,
  )

  test(test, test_runner,
            env: plugin_bluez_tests_env,
    is_parallel: false,
          suite: ['plugins', 'bluez'],
  )
endforeach

