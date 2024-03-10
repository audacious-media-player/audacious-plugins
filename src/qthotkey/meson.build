if get_option('qt5')
  qtx11_dep = dependency('qt5', version: qt_req, required: false, modules: ['X11Extras'])
else
  # QX11Application requires QtGui private headers and Qt >= 6.2
  qtx11_dep = dependency('qt6', version: [qt_req, '>= 6.2'], private_headers: true, required: false, modules: ['Core', 'Gui'])
endif

have_qthotkey = x11_dep.found() and qtx11_dep.found() and not (have_darwin or have_windows)


if have_qthotkey
  shared_module('qthotkey',
    'gui.cc',
    'plugin.cc',
    dependencies: [audacious_dep, qt_dep, audqt_dep, x11_dep, qtx11_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
