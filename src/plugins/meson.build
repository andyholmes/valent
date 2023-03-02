# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>

# Plugins are statically linked
plugins_c_args = []
plugins_link_args = []
plugins_static = []

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
  'gtk',
  'lan',
  'lock',
  'mousepad',
  'mpris',
  'notification',
  'photo',
  'ping',
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

