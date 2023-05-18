# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_lan_test_deps = [
  libvalent_test_dep,
  plugin_lan_deps,
]

plugin_lan_tests = [
  'test-lan-plugin',
]

foreach test : plugin_lan_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_lan_test_deps,
    include_directories: plugin_lan_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_lan],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'lan'],
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

