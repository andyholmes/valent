
# Dependencies
plugin_battery_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_battery_deps,
]

plugin_battery_tests = [
  'test-battery',
  'test-battery-plugin',
  'test-battery-preferences',
  'test-battery-gadget',
]

foreach test : plugin_battery_tests
  test_battery_env = test_env + [
    'BUILDDIR=@0@'.format(meson.current_build_dir()),
    'TEST_PROGRAM=@0@'.format(test),
  ]

  test_program = executable(test, '@0@.c'.format(test),
               c_args: test_c_args,
         dependencies: plugin_battery_test_deps,
            link_args: test_link_args,
           link_whole: [libvalent_test, plugin_battery],
  include_directories: plugin_battery_include_directories,
  )

  test(test, find_program('mock-upower.py'),
            env: test_battery_env,
    is_parallel: false,
          suite: ['plugins', 'battery'],
        timeout: 60,
  )
endforeach

