# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

mock_mutter = find_program('mock_mutter.py')
installed_tests_wrappers += [mock_mutter]
install_data(['remotedesktop.py', 'screencast.py'],
  install_dir: installed_tests_execdir,
)

# Dependencies
plugin_gnome_test_deps = [
  libvalent_test_dep,
  plugin_gnome_deps,
]

plugin_gnome_tests = {
  # FIXME: 'test-mutter-clipboard': mock_mutter,
  'test-mutter-input': mock_mutter,
}

foreach test, test_wrapper : plugin_gnome_tests
  plugin_gnome_tests_env = tests_env + [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
  ]

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_gnome_test_deps,
    include_directories: plugin_gnome_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_gnome],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_wrapper,
            env: plugin_gnome_tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'gnome'],
  )

  installed_tests_plan += [{
    'program': test_program,
    'wrapper': test_wrapper,
  }]
endforeach

