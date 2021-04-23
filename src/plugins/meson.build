
# Plugins are statically linked
plugins_c_args = ['-Wno-missing-declarations']
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
  #'presenter', TODO
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

