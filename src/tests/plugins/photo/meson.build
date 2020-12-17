
# Dependencies
plugin_photo_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_photo_deps,
]

test_program = executable('test-photo-plugin',
                          'test-photo-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_photo_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_photo],
)

test('test-photo-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

