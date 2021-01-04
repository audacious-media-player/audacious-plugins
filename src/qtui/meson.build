qtui_sources = [
  'qtui.cc',
  'dialogs-qt.cc',
  'main_window.cc',
  'menu-ops.cc',
  'menus.cc',
  'playlist-qt.cc',
  'playlist_header.cc',
  'playlist_model.cc',
  'playlist_tabs.cc',
  'info_bar.cc',
  'search_bar.cc',
  'status_bar.cc',
  'tool_bar.cc',
  'time_slider.cc',
  'settings.cc'
]


shared_module('qtui',
  qtui_sources,
  dependencies: [audacious_dep, qt_dep, audqt_dep],
  name_prefix: '',
  install: true,
  install_dir: general_plugin_dir
)
