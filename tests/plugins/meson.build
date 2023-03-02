# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


plugin_tests = [
  # Networking
  'bluez',
  'lan',

  # Device Plugins
  'battery',
  'clipboard',
  'connectivity_report',
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
  'fdo',
  'gtk',
]

foreach plugin : plugin_tests
  if get_option('plugin_' + plugin)
    subdir(plugin)
  endif
endforeach

