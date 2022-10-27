# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_lock_test_deps = [
  libvalent_test_dep,
  plugin_lock_deps,
]

plugin_lock_tests = [
  'test-lock-plugin',
  'test-lock-gadget',
]

foreach test : plugin_lock_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_lock_test_deps,
    include_directories: plugin_lock_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_lock],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['plugins', 'lock'],
  )
endforeach

