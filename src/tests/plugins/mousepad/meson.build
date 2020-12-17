
# Dependencies
plugin_mousepad_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_mousepad_deps,
]

test_program = executable('test-mousepad-plugin',
                          'test-mousepad-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_mousepad_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_mousepad],
)

test('test-mousepad-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

