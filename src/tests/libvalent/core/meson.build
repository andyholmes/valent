# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
core_test_deps = [
  libvalent_core_dep,
  libvalent_test_dep,
]

core_tests = [
  'test-certificate',
  'test-data',
  'test-device',
  'test-manager',
  'test-packet',
  'test-utils',
]

foreach test : core_tests
  source = ['@0@.c'.format(test)]

  test_program = executable(test, source,
          c_args: test_c_args,
    dependencies: core_test_deps,
       link_args: test_link_args,
      link_whole: libvalent_test,
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: 'core',
  )
endforeach

