# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_systemvolume_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_systemvolume_deps,
]

test_program = executable('test-systemvolume-plugin',
                          'test-systemvolume-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_systemvolume_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_systemvolume],
)

test('test-systemvolume-plugin', test_program,
          env: tests_env,
  is_parallel: false,
        suite: ['plugins', 'systemvolume'],
)

