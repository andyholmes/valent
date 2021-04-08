

# Dependencies
plugin_telephony_deps = [
  libvalent_core_dep,
  libvalent_ui_dep,
]

# Sources
plugin_telephony_sources = files([
  'telephony-plugin.c',
  'valent-telephony-plugin.c',
  'valent-telephony-preferences.c',
])

plugin_telephony_include_directories = [include_directories('.')]

# Resources
plugin_telephony_info = i18n.merge_file('telephony.plugin',
   input: 'telephony.plugin.desktop.in',
  output: 'telephony.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_telephony_resources = gnome.compile_resources(
  'telephony-resources',
  'telephony.gresource.xml',
        c_name: 'telephony',
  dependencies: [plugin_telephony_info],
)
plugin_telephony_sources += plugin_telephony_resources

# Settings
install_data(
  ['ca.andyholmes.valent.telephony.gschema.xml'],
  install_dir: schemadir
)

# Static Build
plugin_telephony = static_library('plugin-telephony',
                                  plugin_telephony_sources,
  include_directories: plugin_telephony_include_directories,
         dependencies: plugin_telephony_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_telephony_plugin_register_types']
plugins_static += [plugin_telephony]

