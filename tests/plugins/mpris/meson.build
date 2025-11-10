# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_mpris_test_deps = [
  libvalent_test_dep,
  plugin_mpris_deps,
]

plugin_mpris_tests = [
  'test-mpris-adapter',
  'test-mpris-plugin',
]

foreach test : plugin_mpris_tests
  test_mpris_env = tests_env

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_mpris_test_deps,
    include_directories: plugin_mpris_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_mpris],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: test_mpris_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'mpris'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach
