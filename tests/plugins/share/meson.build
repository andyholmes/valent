# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_share_test_deps = [
  libvalent_test_dep,
  plugin_share_deps,
]

plugin_runcommand_tests = [
  'test-share-download',
  'test-share-plugin',
  'test-share-upload',
]

foreach test : plugin_runcommand_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: tests_c_args,
           dependencies: plugin_share_test_deps,
    include_directories: plugin_share_include_directories,
              link_args: tests_link_args,
             link_whole: [libvalent_test, plugin_share],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'share'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

