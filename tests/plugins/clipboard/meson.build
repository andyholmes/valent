# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_clipboard_test_deps = [
  libvalent_test_dep,
  plugin_clipboard_deps,
]

plugin_clipboard_tests = [
  'test-clipboard-plugin',
]

foreach test : plugin_clipboard_tests
  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_clipboard_test_deps,
    include_directories: plugin_clipboard_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_clipboard],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_program,
           args: ['--tap'],
            env: tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'clipboard'],
  )

  installed_tests_plan += [{
    'program': test_program,
  }]
endforeach

