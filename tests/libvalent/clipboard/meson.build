# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libvalent_clipboard_test_deps = [
  libvalent_test_dep,
]

libvalent_clipboard_tests = [
  'test-clipboard-component',
]

foreach test : libvalent_clipboard_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: libvalent_clipboard_test_deps,
    include_directories: libvalent_include_directories,
              link_args: test_link_args,
             link_whole: libvalent_test,
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['libvalent', 'clipboard'],
  )
endforeach
