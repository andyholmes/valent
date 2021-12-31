# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_fdo_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_fdo_deps,
]

plugin_fdo_tests = {
  'test-fdo-notifications': find_program('mock_notifications.py'),
  'test-fdo-session': find_program('mock_session.py'),
}

foreach test, test_runner : plugin_fdo_tests
  plugin_fdo_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_fdo_test_deps,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_fdo],
    include_directories: plugin_fdo_include_directories,
  )

  test(test, test_runner,
            env: plugin_fdo_tests_env,
    is_parallel: false,
          suite: ['plugins', 'fdo'],
  )
endforeach

