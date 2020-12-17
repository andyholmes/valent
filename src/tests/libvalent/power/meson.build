
# Dependencies
power_component_test_deps = [
  libvalent_core_dep,
  libvalent_power_dep,
  libvalent_test_dep,
]

test_program = executable('test-power-component',
                          'test-power-component.c',
        c_args: test_c_args,
  dependencies: power_component_test_deps,
     link_args: test_link_args,
    link_whole: [libvalent_test],
)

test('test-power-component',
     test_program,
          env: test_env,
  is_parallel: false,
        suite: 'components',
)

