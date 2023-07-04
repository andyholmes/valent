# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Dependencies
plugin_contacts_deps = [
  libvalent_dep,
]

# Sources
plugin_contacts_sources = files([
  'contacts-plugin.c',
  'valent-contacts-plugin.c',
  'valent-contacts-preferences.c',
])

plugin_contacts_include_directories = [include_directories('.')]

# Resources
plugin_contacts_info = i18n.merge_file(
    args: plugins_po_args,
   input: 'contacts.plugin.desktop.in',
  output: 'contacts.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_contacts_resources = gnome.compile_resources('contacts-resources',
                                                    'contacts.gresource.xml',
        c_name: 'contacts',
  dependencies: [plugin_contacts_info],
)
plugin_contacts_sources += plugin_contacts_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.contacts.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_contacts = static_library('plugin-contacts',
                                 plugin_contacts_sources,
    include_directories: plugin_contacts_include_directories,
           dependencies: plugin_contacts_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_contacts]

