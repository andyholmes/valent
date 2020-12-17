
# Dependencies
plugin_notification_test_deps = [
  libvalent_dep,
  libvalent_test_dep,
  plugin_notification_deps,
]

test_program = executable('test-notification-plugin',
                          'test-notification-plugin.c',
        c_args: test_c_args,
  dependencies: plugin_notification_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test, plugin_notification],
)

test('test-notification-plugin',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'plugins',
)

