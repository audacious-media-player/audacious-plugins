gtkui_sources = [
  'columns.cc',
  'layout.cc',
  'menu-ops.cc',
  'menus.cc',
  'ui_infoarea.cc',
  'ui_gtk.cc',
  'ui_playlist_widget.cc',
  'ui_playlist_notebook.cc',
  'ui_statusbar.cc',
  'settings.cc'
]


shared_module('gtkui',
  gtkui_sources,
  dependencies: [audacious_dep, math_dep, gtk_dep, audgui_dep],
  install: true,
  install_dir: general_plugin_dir
)
