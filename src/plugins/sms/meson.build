# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


# Dependencies
plugin_sms_deps = [
  libvalent_dep,

  sqlite_dep,
]

# Sources
plugin_sms_sources = files([
  'sms-plugin.c',
  'valent-contact-row.c',
  'valent-message.c',
  'valent-message-thread.c',
  'valent-sms-conversation.c',
  'valent-sms-conversation-row.c',
  'valent-sms-plugin.c',
  'valent-sms-store.c',
  'valent-sms-utils.c',
  'valent-sms-window.c',

  'valent-date-label.c',
  'valent-message-row.c',
])

plugin_sms_include_directories = [include_directories('.')]

# Resources
plugin_sms_info = i18n.merge_file(
   input: 'sms.plugin.desktop.in',
  output: 'sms.plugin',
  po_dir: po_dir,
    type: 'desktop',
)

plugin_sms_resources = gnome.compile_resources('sms-resources',
                                               'sms.gresource.xml',
        c_name: 'sms',
  dependencies: [plugin_sms_info],
)
plugin_sms_sources += plugin_sms_resources

# Static Build
plugin_sms = static_library('plugin-sms',
                            plugin_sms_sources,
  include_directories: plugin_sms_include_directories,
         dependencies: plugin_sms_deps,
               c_args: plugins_c_args + release_args,
)

#plugins_link_args += ['-Wl,--undefined=valent_sms_plugin_register_types']
plugins_static += [plugin_sms]

