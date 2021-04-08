
# Dependencies
plugin_upower_deps = [
  libvalent_core_dep,
  libvalent_power_dep,
]

# Sources
plugin_upower_sources = files([
  'upower-plugin.c',
  'valent-upower-device.c',
  'valent-upower-device-provider.c',
])

plugin_upower_include_directories = [include_directories('.')]

# Resources
plugin_upower_info = i18n.merge_file('upower.plugin',
   input: 'upower.plugin.desktop.in',
  output: 'upower.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_upower_resources = gnome.compile_resources(
  'upower-resources',
  'upower.gresource.xml',
        c_name: 'upower',
  dependencies: [plugin_upower_info],
)
plugin_upower_sources += plugin_upower_resources

# Static Build
plugin_upower = static_library('plugin-upower',
                               plugin_upower_sources,
  include_directories: plugin_upower_include_directories,
         dependencies: plugin_upower_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_upower_plugin_register_types']
plugins_static += [plugin_upower]

