# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

mock_avahi = find_program('mock_avahi.py')
installed_tests_wrappers += [mock_avahi]
install_data(['avahi.py'],
  install_dir: installed_tests_execdir,
)

# Dependencies
plugin_lan_test_deps = [
  libvalent_test_dep,
  plugin_lan_deps,
]

plugin_lan_tests = {
  'test-lan-plugin': mock_avahi,
  'test-lan-dnssd': mock_avahi,
}

foreach test, test_wrapper : plugin_lan_tests
  plugin_lan_tests_env = [
    'G_TEST_EXE=@0@'.format(join_paths(meson.current_build_dir(), test)),
    'G_TEST_LIBDIR=@0@'.format(get_option('tests_libdir')),
  ]
  foreach envvar : tests_env
    if not (test_wrapper.found() and envvar.startswith('LD_LIBRARY_PATH'))
      plugin_lan_tests_env += envvar
    endif
  endforeach

  test_program = executable(test, '@0@.c'.format(test),
                 c_args: test_c_args,
           dependencies: plugin_lan_test_deps,
    include_directories: plugin_lan_include_directories,
              link_args: test_link_args,
             link_whole: [libvalent_test, plugin_lan],
                install: get_option('installed_tests'),
            install_dir: installed_tests_execdir,
         export_dynamic: true,
  )

  test(test, test_wrapper,
            env: plugin_lan_tests_env,
    is_parallel: false,
       protocol: 'tap',
          suite: ['plugins', 'lan'],
        timeout: tests_timeout,
  )

  installed_tests_plan += [{
    'program': test_program,
    'wrapper': test_wrapper,
  }]
endforeach

