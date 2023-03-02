# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_findmyphone_test_deps = [
  libvalent_test_dep,
  plugin_findmyphone_deps,
]

plugin_findmyphone_tests = [
  'test-findmyphone-plugin',
]

plugin_findmyphone_tests_env = tests_env + [
  'GST_PLUGIN_SYSTEM_PATH_1_0=""',
]

foreach test : plugin_findmyphone_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_findmyphone_test_deps,
    include_directories: plugin_findmyphone_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_findmyphone],
  )

  test(test, test_program,
            env: plugin_findmyphone_tests_env,
    is_parallel: false,
          suite: ['plugins', 'findmyphone'],
  )
endforeach

