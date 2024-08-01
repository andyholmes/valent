# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

mock_modemmanager = find_program('mock_modemmanager.py')
installed_tests_wrappers += [mock_modemmanager]
install_data('modemmanager.py', install_dir: installed_tests_execdir)

# Dependencies
plugin_connectivity_report_test_deps = [
  libvalent_test_dep,
  plugin_connectivity_report_deps,
]

plugin_connectivity_report_tests = {
  'test-connectivity_report-plugin': mock_modemmanager,
  'test-telephony': mock_modemmanager,
}

foreach test, test_wrapper : plugin_connectivity_report_tests
  plugin_connectivity_report_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_connectivity_report_test_deps,
    include_directories: plugin_connectivity_report_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_connectivity_report],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_wrapper,
            env: plugin_connectivity_report_tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'connectivity_report'],
  )

  installed_tests_plan += [{
    'program': test_program,
    'wrapper': test_wrapper,
  }]
endforeach

