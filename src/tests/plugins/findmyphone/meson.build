
# Dependencies
plugin_findmyphone_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_findmyphone_deps,
]

test_program = executable('test-findmyphone-plugin',
                          'test-findmyphone-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_findmyphone_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_findmyphone],
)

test('test-findmyphone-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

