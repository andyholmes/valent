
# Dependencies
plugin_battery_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_battery_deps,
]

plugin_battery_tests = [
  'test-battery-plugin',
  'test-battery-preferences',
  'test-battery-gadget',
]

foreach test : plugin_battery_tests
  source = ['@0@.c'.format(test)]

  test_program = executable(test, source,
          c_args: test_c_args,
    dependencies: plugin_battery_test_deps,
       link_args: test_link_args,
      link_whole: [libvalent_test, plugin_battery],
  )

  test(test,
       test_program,
            env: test_env,
    is_parallel: false,
          suite: 'plugins',
  )
endforeach

