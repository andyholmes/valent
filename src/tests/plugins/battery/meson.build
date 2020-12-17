
# Dependencies
plugin_battery_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_battery_deps,
]

test_program = executable('test-battery-plugin',
                          'test-battery-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_battery_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_battery],
)

test('test-battery-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

