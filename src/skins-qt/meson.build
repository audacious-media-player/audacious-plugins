skins_qt_sources = [
  'actions.cc',
  'button.cc',
  'dialogs-qt.cc',
  'dock.cc',
  'drag-handle.cc',
  'eq-graph.cc',
  'eq-slider.cc',
  'equalizer.cc',
  'hslider.cc',
  'main.cc',
  'menurow.cc',
  'menu-ops.cc',
  'menus.cc',
  'monostereo.cc',
  'number.cc',
  'playlistwin.cc',
  'playlist-slider.cc',
  'playlist-widget.cc',
  'playstatus.cc',
  'plugin.cc',
  'plugin-window.cc',
  'search-select.cc',
  'skin.cc',
  'skin-ini.cc',
  'skins_cfg.cc',
  'skins_util.cc',
  'skinselector.cc',
  'svis.cc',
  'textbox.cc',
  'view.cc',
  'vis-callbacks.cc',
  'vis.cc',
  'widget.cc',
  'window.cc'
]


skins_qt_deps = [audacious_dep, audqt_dep, qt_dep, glib_dep]


if x11_dep.found() and not have_darwin
  if get_option('qt5')
    qtx11_dep = dependency('qt5', version: qt_req, required: false, modules: ['X11Extras'])
  else
    # QX11Application requires QtGui private headers and Qt >= 6.2
    qtx11_dep = dependency('qt6', version: [qt_req, '>= 6.2'], private_headers: true, required: false, modules: ['Core', 'Gui'])
  endif

  if qtx11_dep.found()
    conf.set10('HAVE_QT_X11', true)
    skins_qt_sources += 'x11-utils.cc'
    skins_qt_deps += [x11_dep, qtx11_dep]
  endif
endif


shared_module('skins-qt',
  skins_qt_sources,
  dependencies: skins_qt_deps,
  name_prefix: '',
  install: true,
  install_dir: general_plugin_dir
)
