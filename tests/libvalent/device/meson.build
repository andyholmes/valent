# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libvalent_core_test_deps = [
  libvalent_core_dep,
  libvalent_test_dep,
]

libvalent_core_tests = [
  'test-channel-service',
  'test-device',
  'test-device-manager',
  'test-device-plugin',
  'test-device-transfer',
  'test-packet',
]

foreach test : libvalent_core_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: libvalent_core_test_deps,
    include_directories: libvalent_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, libvalent_core],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['libvalent', 'core'],
  )
endforeach
