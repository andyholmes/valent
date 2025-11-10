# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_runcommand_test_deps = [
  libvalent_test_dep,
  plugin_runcommand_deps,
]

plugin_runcommand_tests = [
  'test-runcommand-plugin',
]

foreach test : plugin_runcommand_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: tests_c_args,
           dependencies: plugin_runcommand_test_deps,
    include_directories: plugin_runcommand_include_directories,
              link_args: tests_link_args,
             link_whole: [libvalent_test, plugin_runcommand],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'runcommand'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

