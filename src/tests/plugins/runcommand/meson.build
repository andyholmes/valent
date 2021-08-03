
# Dependencies
plugin_runcommand_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_runcommand_deps,
]

plugin_runcommand_tests = [
  'test-runcommand-plugin',
  'test-runcommand-preferences',
]

foreach test : plugin_runcommand_tests
  source = ['@0@.c'.format(test)]

  test_program = executable(test, source,
          c_args: test_c_args,
    dependencies: plugin_runcommand_test_deps,
       link_args: test_link_args,
      link_whole: [libvalent_test, plugin_runcommand],
  )

  test(test,
       test_program,
            env: test_env,
    is_parallel: false,
          suite: ['plugins', 'runcommand'],
  )
endforeach

