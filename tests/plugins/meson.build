# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com>


plugin_tests = [
  'battery',
  'bluez',
  'clipboard',
  'connectivity_report',
  'contacts',
  # 'eds',
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
  # 'pipewire',
  'presenter',
  # 'pulseaudio',
  'runcommand',
  'sftp',
  'share',
  'sms',
  'systemvolume',
  'telephony',
  # 'xdp',
]

if get_option('plugins')
  foreach plugin : plugin_tests
    if get_option('plugin_@0@'.format(plugin))
      subdir(plugin)
    endif
  endforeach
endif

