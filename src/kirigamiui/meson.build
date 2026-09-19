qt_quick_dep = dependency(
  'qt6',
  version: qt_req,
  required: true,
  modules: ['Qml', 'Quick', 'QuickControls2']
)

qt6_module = import('qt6')
kirigamiui_generated = qt6_module.preprocess(
  moc_headers: ['mobile_ui.h'],
  qresources: ['kirigamiui.qrc'],
  dependencies: [audacious_dep, qt_dep, qt_quick_dep]
)

shared_module('kirigamiui',
  'kirigamiui.cc',
  'mobile_ui.cc',
  'native_widget_item.cc',
  kirigamiui_generated,
  dependencies: [audacious_dep, qt_dep, qt_quick_dep, audqt_dep],
  name_prefix: '',
  install: true,
  install_dir: general_plugin_dir
)
