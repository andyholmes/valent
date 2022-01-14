# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_battery_test_deps = [
  libvalent_test_dep,
  plugin_battery_deps,
]

plugin_battery_tests = [
  'test-battery',
  'test-battery-plugin',
  'test-battery-preferences',
  'test-battery-gadget',
]

plugin_battery_test_wrapper = find_program('mock_upower.py')

foreach test : plugin_battery_tests
  plugin_battery_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_battery_test_deps,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_battery],
    include_directories: plugin_battery_include_directories,
  )

  test(test, plugin_battery_test_wrapper,
            env: plugin_battery_tests_env,
    is_parallel: false,
          suite: ['plugins', 'battery'],
  )
endforeach

