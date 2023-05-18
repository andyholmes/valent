# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libvalent_device_test_deps = [
  libvalent_test_dep,
]

libvalent_device_tests = [
  'test-channel-service',
  'test-device',
  'test-device-manager',
  'test-device-plugin',
  'test-device-transfer',
  'test-packet',
]

foreach test : libvalent_device_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: libvalent_device_test_deps,
              link_args: test_link_args,
             link_whole: libvalent_test,
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['libvalent', 'device'],
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

