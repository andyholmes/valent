
# Dependencies
plugin_upower_test_deps = [
  libvalent_core_dep,
  libvalent_power_dep,
  libvalent_test_dep,
]

test = 'test-upower'
source = '@0@.c'.format(test)

test_program = executable(test, source,
include_directories: plugin_upower_include_directories,
        c_args: test_c_args,
  dependencies: plugin_upower_test_deps,
    link_whole: [libvalent_test, plugin_upower],
)

upower_env = test_env + [
  'BUILDDIR=@0@'.format(meson.current_build_dir()),
  'SOURCEDIR=@0@'.format(meson.current_source_dir()),
]

test('test-upower', find_program('test-upower.py'),
          env: upower_env,
  is_parallel: false,
        suite: 'plugins',
      timeout: 60,
)

