# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_ping_test_deps = [
  libvalent_test_dep,
  plugin_ping_deps,
]

plugin_ping_tests = [
  'test-ping-plugin',
]

foreach test : plugin_ping_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_ping_test_deps,
    include_directories: plugin_ping_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_ping],
  )

  test(test, test_program,
           args: ['--tap'],
            env: tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'ping'],
  )
endforeach

