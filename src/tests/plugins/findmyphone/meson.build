# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_findmyphone_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_findmyphone_deps,
]

test_program = executable('test-findmyphone-plugin',
                          'test-findmyphone-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_findmyphone_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_findmyphone],
)

plugin_findmyphone_tests_env = tests_env + [
  'GST_PLUGIN_SYSTEM_PATH_1_0=""',
]

test('test-findmyphone-plugin', test_program,
          env: plugin_findmyphone_tests_env,
  is_parallel: false,
        suite: ['plugins', 'findmyphone'],
)

