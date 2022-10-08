# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Test Environment
libpeas_libdir = libpeas_dep.get_variable('libdir')

typelib_dirs = [
  join_paths(libpeas_libdir, 'girepository-1.0'),
  join_paths(meson.project_build_root(), 'src', 'libvalent'),
]

tests_env = [
  'MALLOC_CHECK_=3',
  'GI_TYPELIB_PATH=@0@:$(GI_TYPELIB_PATH)'.format(':'.join(typelib_dirs)),
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  # https://docs.gtk.org/glib/running.html
  'G_DEBUG=gc-friendly',
  'G_SLICE=always-malloc',
  # https://docs.gtk.org/gio/overview.html
  'GIO_USE_VFS=local',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/schemas'.format(meson.project_build_root()),
  'GVFS_DISABLE_FUSE=true',
  # https://docs.gtk.org/gtk4/running.html
  'GDK_BACKEND=wayland,x11',
  'GTK_A11Y=test',
  # See: https://github.com/google/sanitizers/issues/1322
  'ASAN_OPTIONS=detect_leaks=1,intercept_tls_get_addr=0',
  'LSAN_OPTIONS=fast_unwind_on_malloc=0,suppressions=@0@'.format(
    join_paths(meson.current_source_dir(), 'extra', 'lsan.supp')),
  'TSAN_OPTIONS=force_seq_cst_atomics=1,history_size=5,suppressions=@0@'.format(
    join_paths(meson.current_source_dir(), 'extra', 'tsan.supp')),
  'UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1',
]

test_c_args = [
  '-I@0@'.format(join_paths(meson.project_source_root(), 'src')),
  '-DJSON_SCHEMA_DIR="@0@"'.format(
    join_paths(meson.project_source_root(), 'doc', 'schemas')),
  '-DTEST_DATA_DIR="@0@"'.format(
    join_paths (meson.current_source_dir(), 'data')),
]

test_link_args = [
]


# Copy GSettingsSchemas for testing
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

