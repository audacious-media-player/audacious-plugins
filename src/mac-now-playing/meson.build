mac_now_playing_sources = [
  'MacNowPlaying.mm'
]


shared_module('mac-now-playing',
  mac_now_playing_sources,
  dependencies: [audacious_dep],
  name_prefix: '',
  objc_args: ['-std=c99', '-fobjc-exceptions'],
  objcpp_args: '-std=c++11',
  link_args: ['-framework', 'AppKit', '-framework', 'MediaPlayer'],
  install: true,
  install_dir: general_plugin_dir
)
