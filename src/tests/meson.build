
#
# Test Fixtures
#
subdir('fixtures')


# Test Environment
libpeas_libdir = libpeas_dep.get_pkgconfig_variable('libdir')

typelib_dirs = [
  '@0@/src/libvalent'.format(meson.build_root()),
  join_paths(libpeas_libdir, 'girepository-1.0'),
]

test_env = [
  'VALENT_TEST=1',
  'GI_TYPELIB_PATH=@0@:$(GI_TYPELIB_PATH)'.format(':'.join(typelib_dirs)),
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
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
  'UBSAN_OPTIONS=print_stacktrace=1:print_summary=1:halt_on_error=1',
]

test_c_args = [
  '-DVALENT_TEST=1',
  '-DTEST_DATA_DIR="@0@/data/"'.format(meson.current_source_dir()),
  '-I' + join_paths(meson.source_root(), 'src'),
]

test_link_args = [
]


# Copy Plugin GSchemas
test_schema_dir = join_paths(meson.build_root(), 'schemas')
plugins_gschemas = run_command(find_program('plugin_schemas.sh'))


# Validate GSchemas
compile_schemas = find_program('glib-compile-schemas', required: false)

if compile_schemas.found()
  test('Validate GSchema files', compile_schemas,
    args: ['--strict', '--dry-run', test_schema_dir]
  )
endif


# Compile GSchemas for unit tests
gschemas_compiled = custom_target('gschemas.compiled',
            output: 'gschemas.compiled',
           command: ['glib-compile-schemas', test_schema_dir],
  build_by_default: true,
           install: false,
)


# Run DBus tests
dbus_tests = get_option('dbus_tests')
dbus_run_session = find_program('dbus-run-session', required: dbus_tests)

if dbus_tests and dbus_run_session.found()
  test_c_args += ['-DVALENT_TEST_DBUS=@0@'.format(dbus_tests.to_int())]
endif


# Run fuzzing tests
fuzz_tests = get_option('fuzz_tests')

if fuzz_tests and get_option('b_sanitize') == 'none'
  test_c_args += ['-DVALENT_TEST_FUZZ=@0@'.format(fuzz_tests.to_int())]
endif


# Source Tests
subdir('libvalent')
subdir('plugins')

