# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
mixer_component_test_deps = [
  libvalent_core_dep,
  libvalent_mixer_dep,
  libvalent_test_dep,
]

test_program = executable('test-mixer-component',
                          'test-mixer-component.c',
        c_args: test_c_args,
  dependencies: mixer_component_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test],
)

test('test-mixer-component', test_program,
          env: tests_env,
  is_parallel: false,
        suite: ['components', 'mixer'],
)

