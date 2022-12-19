# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_mpris_test_deps = [
  libvalent_test_dep,
  plugin_mpris_deps,
]

plugin_mpris_tests = [
  'test-mpris-component',
  'test-mpris-impl',
  'test-mpris-plugin',
  'test-mpris-remote',
]

foreach test : plugin_mpris_tests
  test_mpris_env = tests_env

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_mpris_test_deps,
    include_directories: plugin_mpris_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_mpris],
  )

  test(test, test_program,
            env: test_mpris_env,
    is_parallel: false,
          suite: ['plugins', 'mpris'],
  )
endforeach
