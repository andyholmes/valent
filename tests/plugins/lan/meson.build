# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_lan_test_deps = [
  libvalent_core_dep,
  libvalent_test_dep,
]

plugin_lan_tests = [
  'test-lan-plugin',
]

foreach test : plugin_lan_tests
  test_program = executable(test, '@0@.c'.format(test),
               c_args: test_c_args,
         dependencies: plugin_lan_test_deps,
  include_directories: plugin_lan_include_directories,
            link_args: test_link_args,
           link_whole: [libvalent_test, plugin_lan],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['plugins', 'lan'],
  )
endforeach

