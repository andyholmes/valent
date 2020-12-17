
# Backends (Channel Providers)
backend_tests = [
  #'bluetooth',
  'lan',
]

foreach plugin : backend_tests
  subdir(plugin)
endforeach


# Plugins (Device Plugins)
plugin_tests = [
 'battery',
 'clipboard',
 'contacts',
 'findmyphone',
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

