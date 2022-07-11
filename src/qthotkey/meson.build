if get_option('qt6')
  qtx11extras_dep = dependency('qt6', version: qt_req, required: false, modules: ['X11Extras'])
else
  qtx11extras_dep = dependency('qt5', version: qt_req, required: false, modules: ['X11Extras'])
endif

have_qthotkey = qtx11extras_dep.found()


if have_qthotkey
  shared_module('qthotkey',
    'gui.cc',
    'plugin.cc',
    dependencies: [audacious_dep, qt_dep, audqt_dep, qtx11extras_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
