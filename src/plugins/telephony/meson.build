# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


# Dependencies
plugin_telephony_deps = [
  libvalent_dep,

  gdk_pixbuf_dep,
]

# Sources
plugin_telephony_sources = files([
  'telephony-plugin.c',
  'valent-telephony-plugin.c',
])

plugin_telephony_include_directories = [include_directories('.')]

# Resources
plugin_telephony_info = i18n.merge_file(
    args: plugins_po_args,
   input: configure_file(
            input: files('telephony.plugin.desktop.in'),
           output: 'telephony.plugin.desktop',
    configuration: plugins_conf,
  ),
  output: 'telephony.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_telephony_resources = gnome.compile_resources('telephony-resources',
                                                     'telephony.gresource.xml',
        c_name: 'telephony',
  dependencies: [plugin_telephony_info],
)
plugin_telephony_sources += plugin_telephony_resources

# Settings
install_data('ca.andyholmes.Valent.Plugin.telephony.gschema.xml',
  install_dir: schemadir
)

# Static Build
plugin_telephony = static_library('plugin-telephony',
                                  plugin_telephony_sources,
    include_directories: plugin_telephony_include_directories,
           dependencies: plugin_telephony_deps,
                 c_args: plugins_c_args + release_args,
  gnu_symbol_visibility: 'hidden',
)

plugins_static += [plugin_telephony]

