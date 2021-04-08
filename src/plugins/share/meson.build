
# Dependencies
plugin_share_deps = [
  libvalent_core_dep,
  gtk_dep,
]

# Sources
plugin_share_sources = files([
  'share-plugin.c',
  'valent-share-plugin.c',
])

plugin_share_include_directories = [include_directories('.')]

# Resources
plugin_share_info = i18n.merge_file('share.plugin',
   input: 'share.plugin.desktop.in',
  output: 'share.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_share_resources = gnome.compile_resources(
  'share-resources',
  'share.gresource.xml',
        c_name: 'share',
  dependencies: [plugin_share_info],
)
plugin_share_sources += plugin_share_resources

# Static Build
plugin_share = static_library('plugin-share',
                              plugin_share_sources,
  include_directories: plugin_share_include_directories,
         dependencies: plugin_share_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_share_plugin_register_types']
plugins_static += [plugin_share]

