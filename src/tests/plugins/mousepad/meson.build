# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_mousepad_test_deps = [
  libvalent_test_dep,
  plugin_mousepad_deps,
]

plugin_mousepad_tests = [
  'test-mousepad-plugin',
  'test-mousepad-remote',
]

foreach test : plugin_mousepad_tests
  test_mousepad_env = tests_env

  test_program = executable(test, '@0@.c'.format(test),
               c_args: test_c_args,
         dependencies: plugin_mousepad_test_deps,
  include_directories: plugin_mousepad_include_directories,
            link_args: test_link_args,
           link_whole: [libvalent_test, plugin_mousepad],
  )

  test(test, test_program,
            env: test_mousepad_env,
    is_parallel: false,
          suite: ['plugins', 'mousepad'],
  )
endforeach

