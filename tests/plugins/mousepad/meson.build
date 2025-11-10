# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_mousepad_test_deps = [
  libvalent_test_dep,
  plugin_mousepad_deps,
]

plugin_mousepad_tests = [
  'test-mousepad-plugin',
]

foreach test : plugin_mousepad_tests
  test_mousepad_env = tests_env

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: tests_c_args,
           dependencies: plugin_mousepad_test_deps,
    include_directories: plugin_mousepad_include_directories,
              link_args: tests_link_args,
             link_whole: [libvalent_test, plugin_mousepad],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: test_mousepad_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'mousepad'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

