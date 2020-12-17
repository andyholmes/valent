
# Dependencies
plugin_contacts_deps = [
  libvalent_core_dep,
  libvalent_contacts_dep,
  libvalent_ui_dep,
]

# Sources
plugin_contacts_sources = files([
  'contacts-plugin.c',
  'valent-contacts-plugin.c',
  'valent-contacts-preferences.c',
])

plugin_contacts_include_directories = [include_directories('.')]

# Resources
plugin_contacts_resources = gnome.compile_resources(
  'contacts-resources',
  'contacts.gresource.xml',
  c_name: 'contacts',
)
plugin_contacts_sources += plugin_contacts_resources

# Settings
install_data(
  ['ca.andyholmes.valent.contacts.gschema.xml'],
  install_dir: schemadir
)

# Static Build
plugin_contacts = static_library('plugin-contacts',
                                 plugin_contacts_sources,
  include_directories: plugin_contacts_include_directories,
         dependencies: plugin_contacts_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_contacts_plugin_register_types']
plugins_static += [plugin_contacts]

