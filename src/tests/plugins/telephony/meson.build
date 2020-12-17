
# Dependencies
plugin_telephony_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_telephony_deps,
]

test_program = executable('test-telephony-plugin',
                          'test-telephony-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_telephony_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_telephony],
)

test('test-telephony-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

