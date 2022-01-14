# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_ping_test_deps = [
  libvalent_test_dep,
  plugin_ping_deps,
]

test_program = executable('test-ping-plugin',
                          'test-ping-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_ping_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_ping],
)

test('test-ping-plugin', test_program,
          env: tests_env,
  is_parallel: false,
        suite: ['plugins', 'ping'],
)

