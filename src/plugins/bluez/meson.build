
# Dependencies
plugin_bluez_deps = [
  libvalent_core_dep,
]

# Sources
plugin_bluez_sources = files([
  'bluez-plugin.c',
  'valent-bluez-channel-service.c',
  'valent-bluez-channel.c',
  'valent-bluez-device.c',
  'valent-bluez-profile.c',
  'valent-mux-connection.c',
  'valent-mux-input-stream.c',
  'valent-mux-output-stream.c',
])

plugin_bluez_include_directories = [include_directories('.')]

# Resources
plugin_bluez_resources = gnome.compile_resources(
  'bluez-resources',
  'bluez.gresource.xml',
  c_name: 'bluez',
)
plugin_bluez_sources += plugin_bluez_resources

# Settings
install_data(
  ['ca.andyholmes.valent.bluez.gschema.xml'],
  install_dir: schemadir
)

# Static Build
plugin_bluez = static_library('plugin-bluez',
                              plugin_bluez_sources,
  include_directories: plugin_bluez_include_directories,
         dependencies: plugin_bluez_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_bluez_plugin_register_types']
plugins_static += [plugin_bluez]

