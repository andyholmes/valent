# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Test Environment
typelib_dirs = [
  join_paths(libpeas_dep.get_variable('libdir'), 'girepository-1.0'),
  join_paths(meson.project_build_root(), 'src', 'libvalent'),
]

tests_env = [
  'MALLOC_CHECK_=3',
  # https://docs.gtk.org/glib/running.html
  'G_DEBUG=gc-friendly',
  'G_ENABLE_DIAGNOSTIC=1',
  # https://docs.gtk.org/gio/overview.html
  'GIO_USE_NETWORK_MONITOR=valent',
  'GIO_USE_VFS=local',
  'GIO_USE_VOLUME_MONITOR=unix',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/schemas'.format(meson.project_build_root()),
  'GVFS_DISABLE_FUSE=true',
  # https://gnome.pages.gitlab.gnome.org/gobject-introspection/girepository/GIRepository.html
  'GI_TYPELIB_PATH=@0@:$(GI_TYPELIB_PATH)'.format(':'.join(typelib_dirs)),
  # https://docs.gtk.org/gtk4/running.html
  'GDK_BACKEND=wayland,x11',
  'GDK_DEBUG=default-settings',
  'GSK_RENDERER=opengl',
  'GTK_A11Y=test',
  # https://github.com/google/sanitizers/wiki/SanitizerCommonFlags
  # https://github.com/google/sanitizers/wiki/AddressSanitizerFlags
  # https://github.com/google/sanitizers/wiki/ThreadSanitizerFlags
  'ASAN_OPTIONS=detect_leaks=1,' +
               'intercept_tls_get_addr=0',
  'LSAN_OPTIONS=fast_unwind_on_malloc=0,' +
               'suppressions=@0@'.format(
    join_paths(meson.current_source_dir(), 'extra', 'lsan.supp')),
  'TSAN_OPTIONS=force_seq_cst_atomics=1,' +
               'history_size=5,' +
               'ignore_noninstrumented_modules=1,' +
               'suppressions=@0@'.format(
    join_paths(meson.current_source_dir(), 'extra', 'tsan.supp')),
  'UBSAN_OPTIONS=halt_on_error=1,' +
                'print_stacktrace=1',
  'PYTHONDONTWRITEBYTECODE=yes',
]

test_c_args = [
]

test_link_args = [
]

tests_timeout = 30
if get_option('b_sanitize') != 'none'
  tests_timeout = tests_timeout * 3
endif

installed_tests_plan = []
installed_tests_wrappers = []
installed_tests_execdir = join_paths(libexecdir, 'installed-tests', libvalent_api_name)
installed_tests_datadir = join_paths(datadir, 'installed-tests', libvalent_api_name)
installed_tests_env = [
  'MALLOC_CHECK_=3',
  # https://docs.gtk.org/glib/running.html
  'G_DEBUG=gc-friendly',
  'G_SLICE=always-malloc',
  # https://docs.gtk.org/gio/overview.html
  'GIO_USE_VFS=local',
  'GIO_USE_VOLUME_MONITOR=unix',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@'.format(join_paths(datadir, 'glib-2.0', 'schemas')),
  'GVFS_DISABLE_FUSE=true',
  # https://docs.gtk.org/gtk4/running.html
  'GDK_DEBUG=default-settings',
  'GTK_A11Y=test',
]
installed_tests_tmpl = join_paths(meson.current_source_dir(), 'extra',
  'template.test.in')

# Instrumented Libraries
tests_libdir = get_option('tests_libdir')
if tests_libdir != ''
  printenv = find_program('printenv', required: false)
  if printenv.found()
    res = run_command(printenv, 'LD_LIBRARY_PATH')
    if res.returncode() == 0
      tests_libdir = '@0@:@1@'.format(tests_libdir, res.stdout().strip())
    endif
  endif

  tests_env += 'LD_LIBRARY_PATH=@0@'.format(tests_libdir)
  installed_tests_env += 'LD_LIBRARY_PATH=@0@'.format(tests_libdir)
endif

# Optional dependencies
if glycin_dep.found()
  # https://gitlab.gnome.org/GNOME/glycin/-/blob/main/CONTRIBUTING.md
  tests_env += 'GLYCIN_DATA_DIR=@0@/share'.format(glycin_dep.get_variable('prefix'))
  installed_tests_env += 'GLYCIN_DATA_DIR=@0@/share'.format(glycin_dep.get_variable('prefix'))
endif

# Compile GSettings schemas for in-tree testing
gsettings_test_init = find_program('extra/gsettings-test-init.sh')

run_command(gsettings_test_init,
    env: tests_env,
  check: true,
)

tests_schema_dir = join_paths(meson.project_build_root(), 'schemas')

gschemas_compiled = custom_target('gschemas.compiled',
            output: 'gschemas.compiled',
           command: ['glib-compile-schemas', tests_schema_dir],
  build_by_default: true,
           install: false,
)

test('Validate GSchema files', find_program('glib-compile-schemas'),
         args: ['--strict', '--dry-run', tests_schema_dir],
          env: tests_env,
  is_parallel: false,
)


subdir('fixtures')
subdir('libvalent')
subdir('plugins')


# Installed Tests
if get_option('installed_tests')
  foreach test_wrapper : installed_tests_wrappers
    install_data(test_wrapper.full_path(),
       install_dir: installed_tests_execdir,
      install_mode: 'rwxr-xr-x',
    )
  endforeach

  foreach test_plan : installed_tests_plan
    test_program = test_plan.get('program')
    test_name = test_program.name()
    test_exec = test_program.name()
    test_env = installed_tests_env

    if test_plan.has_key('wrapper')
      test_wrapper = test_plan.get('wrapper')
      test_exec = test_wrapper.full_path().split('/')[-1]
      test_env += [
        'G_TEST_EXE=@0@'.format(join_paths(installed_tests_execdir, test_name)),
      ]
    endif

    test_conf = configuration_data()
    test_conf.set('installed_tests_type', 'session')
    test_conf.set('installed_tests_execdir', installed_tests_execdir)
    test_conf.set('installed_tests_exec', test_exec)
    test_conf.set('installed_tests_env', ' '.join(test_env))

    configure_file(
              input: installed_tests_tmpl,
             output: '@0@.test'.format(test_name),
      configuration: test_conf,
            install: true,
        install_dir: installed_tests_datadir,
    )
  endforeach
endif

