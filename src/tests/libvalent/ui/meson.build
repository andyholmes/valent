# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
ui_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
]

ui_tests = [
  'test-device-panel',
  'test-panel',
  'test-plugin-group',
  'test-plugin-row',
  'test-window',
]

foreach test : ui_tests
  source = ['@0@.c'.format(test)]

  test_program = executable(test, source,
          c_args: test_c_args,
    dependencies: ui_test_deps,
       link_args: test_link_args,
      link_whole: [libvalent_test, libvalent_ui],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: 'ui',
  )
endforeach
