# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2022 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
libvalent_mixer_test_deps = [
  libvalent_core_dep,
  libvalent_mixer_dep,
  libvalent_test_dep,
]

libvalent_mixer_tests = [
  'test-mixer-component',
]

foreach test : libvalent_mixer_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: libvalent_mixer_test_deps,
    include_directories: libvalent_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, libvalent_mixer],
  )

  test(test, test_program,
            env: tests_env,
    is_parallel: false,
          suite: ['libvalent', 'mixer'],
  )
endforeach
