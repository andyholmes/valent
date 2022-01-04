# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


plugin_tests = [
  # Networking
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
  'runcommand',
  'sftp',
  'share',
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

