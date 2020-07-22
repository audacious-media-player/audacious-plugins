moonstone_sources = [
  'moonstone.cc',
  'main_window.cc',
  'info_bar.cc',
  'tool_bar.cc',
  'time_slider.cc',
  'playlist.cc',
  'playlist_header.cc',
  'playlist_model.cc',
  'playlist_tabs.cc',
  'playlist_selection.cc'
]


shared_module('moonstone',
  moonstone_sources,
  dependencies: [audacious_dep, qt_dep, audqt_dep],
  install: true,
  install_dir: general_plugin_dir
)
