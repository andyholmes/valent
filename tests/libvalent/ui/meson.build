# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libvalent_ui_test_deps = [
  libvalent_core_dep,
  libvalent_ui_dep,
  libvalent_test_dep,
]

libvalent_ui_tests = [
  'test-application',
  'test-device-panel',
  'test-device-preferences-page',
  'test-device-preferences-window',
  'test-preferences-page',
  'test-preferences-window',
  'test-window',
]

foreach test : libvalent_ui_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: libvalent_ui_test_deps,
    include_directories: libvalent_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, libvalent_ui],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['libvalent', 'ui'],
  )
endforeach
