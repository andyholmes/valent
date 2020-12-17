
# Dependencies
plugin_contacts_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_contacts_deps,
]

test_program = executable('test-contacts-plugin',
                          'test-contacts-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_contacts_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_contacts],
)

test('test-contacts-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

