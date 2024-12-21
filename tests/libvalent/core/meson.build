# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libvalent_core_test_deps = [
  libvalent_test_dep,
]

libvalent_core_tests = [
  'test-application',
  'test-application-plugin',
  'test-context',
  'test-data-source',
  'test-object',
  'test-resource',
  'test-utils',
]

foreach test : libvalent_core_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: libvalent_core_test_deps,
              link_args: test_link_args,
             link_whole: libvalent_test,
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['libvalent', 'core'],
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

