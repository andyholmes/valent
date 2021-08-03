
# Dependencies
plugin_share_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_share_deps,
]

test_program = executable('test-share-plugin',
                          'test-share-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_share_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_share],
)

test('test-share-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: ['plugins', 'share'],
)

