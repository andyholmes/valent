# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>

# Networking (Channel Services)
network_tests = [
  #'bluetooth',
  'lan',
]

foreach plugin : network_tests
  subdir(plugin)
endforeach


# Plugins (Device Plugins)
plugin_tests = [
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
 'share',
 'systemvolume',
 'telephony',
]

foreach plugin : plugin_tests
  if get_option('plugin_' + plugin)
    subdir(plugin)
  endif
endforeach

