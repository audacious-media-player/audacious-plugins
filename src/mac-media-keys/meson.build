mac_media_keys_sources = [
  'MacMediaKeys.mm',
  'SPInvocationGrabbing.m',
  'SPMediaKeyTap.m'
]


shared_module('mac-media-keys',
  mac_media_keys_sources,
  dependencies: [audacious_dep],
  name_prefix: '',
  objcpp_args: '-std=c++11',
  link_args: ['-framework', 'AppKit', '-framework', 'Carbon'],
  install: true,
  install_dir: general_plugin_dir
)
