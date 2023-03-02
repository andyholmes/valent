# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_connectivity_report_test_deps = [
  libvalent_test_dep,
  plugin_connectivity_report_deps,
]

plugin_connectivity_report_tests = [
  'test-connectivity_report-plugin',
  'test-connectivity_report-preferences',
  'test-connectivity_report-gadget',
  'test-telephony',
]

plugin_connectivity_report_test_wrapper = find_program('mock_modemmanager.py')

foreach test : plugin_connectivity_report_tests
  plugin_connectivity_report_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_connectivity_report_test_deps,
    include_directories: plugin_connectivity_report_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_connectivity_report],
  )

  test(test, plugin_connectivity_report_test_wrapper,
            env: plugin_connectivity_report_tests_env,
    is_parallel: false,
          suite: ['plugins', 'connectivity_report'],
  )
endforeach

