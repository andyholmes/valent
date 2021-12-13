# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Test Environment
libpeas_libdir = libpeas_dep.get_pkgconfig_variable('libdir')

typelib_dirs = [
  '@0@/src/libvalent'.format(meson.build_root()),
  join_paths(libpeas_libdir, 'girepository-1.0'),
]

tests_env = [
  'GI_TYPELIB_PATH=@0@:$(GI_TYPELIB_PATH)'.format(':'.join(typelib_dirs)),
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'G_SLICE=always-malloc',
  'GSETTINGS_BACKEND=memory',
  'GSETTINGS_SCHEMA_DIR=@0@/schemas'.format(meson.build_root()),
  'PYTHONDONTWRITEBYTECODE=yes',
  'MALLOC_CHECK_=2',
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
  '-DTEST_DATA_DIR="@0@/data/"'.format(meson.current_source_dir()),
  '-DJSON_SCHEMA_DIR="@0@"'.format(join_paths(meson.source_root(), 'doc', 'schemas')),
  '-I' + join_paths(meson.source_root(), 'src'),
]

test_link_args = [
]


# Copy GSettingsSchemas for testing
gsettings_test_init = find_program('gsettings-test-init.sh')

run_command(gsettings_test_init,
    env: tests_env,
  check: true,
)

compile_schemas = find_program('glib-compile-schemas')
test_schema_dir = join_paths(meson.build_root(), 'schemas')

test('Validate GSchema files', compile_schemas,
  args: ['--strict', '--dry-run', test_schema_dir],
)

gschemas_compiled = custom_target('gschemas.compiled',
            output: 'gschemas.compiled',
           command: ['glib-compile-schemas', test_schema_dir],
  build_by_default: true,
           install: false,
)


# Run DBus tests
if get_option('dbus_tests')
  test_c_args += ['-DVALENT_TEST_DBUS=1']
endif


# Run fuzzing tests (exclusive to sanitizers)
if get_option('fuzz_tests') and get_option('b_sanitize') == 'none'
  test_c_args += ['-DVALENT_TEST_FUZZ=1']
endif


subdir('fixtures')
subdir('libvalent')
subdir('plugins')

