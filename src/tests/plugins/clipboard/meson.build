
# Dependencies
plugin_clipboard_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_clipboard_deps,
]

test_program = executable('test-clipboard-plugin',
                          'test-clipboard-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_clipboard_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_clipboard],
)

test('test-clipboard-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

