# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Plugins are statically linked
plugins_c_args = []
plugins_link_args = []
plugins_static = []

# Peas Plugins
plugins = [
  # Networking
  'bluez',
  'lan',

  # Device Plugins
  'battery',
  'clipboard',
  'contacts',
  'findmyphone',
  'lock',
  'mousepad',
  'mpris',
  'notification',
  'photo',
  'ping',
  'presenter',
  'runcommand',
  'sftp',
  'share',
  'sms',
  'systemvolume',
  'telephony',

  # Integration
  'eds',
  'fdo',
  'gtk',
  'pulseaudio',
  'xdp',
]

# Each plugin has it's own build
foreach plugin : plugins
  if get_option('plugin_' + plugin)
    subdir(plugin)
  endif
endforeach

