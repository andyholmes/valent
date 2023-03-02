# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libvalent_core_test_deps = [
  libvalent_test_dep,
]

libvalent_core_tests = [
  'test-certificate',
  'test-context',
  'test-object',
  'test-utils',
]

foreach test : libvalent_core_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: libvalent_core_test_deps,
              link_args: test_link_args,
             link_whole: libvalent_test,
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['libvalent', 'core'],
  )
endforeach

