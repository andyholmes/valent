# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Plugins are statically linked
plugins_c_args = []
plugins_link_args = []
plugins_static = []

# NOTE: Pass to `i18n.merge_file()` for `.plugin` files
plugins_po_args = [
    '--keyword=Name',
    '--keyword=Description'
]

# Peas Plugins
plugins = [
  'battery',
  'bluez',
  'clipboard',
  'connectivity_report',
  'contacts',
  'eds',
  'fdo',
  'findmyphone',
  'gnome',
  'gtk',
  'lan',
  'lock',
  'mousepad',
  'mpris',
  'notification',
  'ping',
  'pipewire',
  'presenter',
  'pulseaudio',
  'runcommand',
  'sftp',
  'share',
  'sms',
  'systemvolume',
  'telephony',
  'xdp',
]

if get_option('plugins')
  foreach plugin : plugins
    if get_option('plugin_@0@'.format(plugin))
      subdir(plugin)
    endif
  endforeach
endif

