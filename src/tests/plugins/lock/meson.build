# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_lock_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_lock_deps,
]

test_program = executable('test-lock-plugin',
                          'test-lock-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_lock_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_lock],
)

test('test-lock-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: ['plugins', 'lock'],
)

