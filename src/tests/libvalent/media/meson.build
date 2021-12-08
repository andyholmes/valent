# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
media_component_test_deps = [
  libvalent_core_dep,
  libvalent_media_dep,
  libvalent_test_dep,
]

test_program = executable('test-media-component',
                          'test-media-component.c',
        c_args: test_c_args,
  dependencies: media_component_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test],
)

test('test-media-component',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: ['components', 'media'],
)

