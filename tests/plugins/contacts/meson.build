# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_contacts_test_deps = [
  libvalent_test_dep,
  plugin_contacts_deps,
]

plugin_contacts_tests = [
  'test-contacts-plugin',
]

foreach test : plugin_contacts_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_contacts_test_deps,
    include_directories: plugin_contacts_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_contacts],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'contacts'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

