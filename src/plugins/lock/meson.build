
# Dependencies
plugin_lock_deps = [
  libvalent_core_dep,
  libvalent_session_dep,
]

# Sources
plugin_lock_sources = files([
  'lock-plugin.c',
  'valent-lock-plugin.c',
])

plugin_lock_include_directories = [include_directories('.')]

# Resources
plugin_lock_resources = gnome.compile_resources(
  'lock-resources',
  'lock.gresource.xml',
  c_name: 'lock',
)
plugin_lock_sources += plugin_lock_resources

# Static Build
plugin_lock = static_library('plugin-lock',
                             plugin_lock_sources,
  include_directories: plugin_lock_include_directories,
         dependencies: plugin_lock_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_lock_plugin_register_types']
plugins_static += [plugin_lock]

