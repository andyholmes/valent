
# Dependencies
plugin_mpris_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_mpris_deps,
]

test_program = executable('test-mpris-plugin',
                          'test-mpris-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_mpris_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_mpris],
  include_directories: plugin_mpris_include_directories,
)

test('test-mpris-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: ['plugins', 'mpris'],
)

