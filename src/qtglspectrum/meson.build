qt_opengl_dep = dependency('qt5',
  version: '>= 5.4',
  required: get_option('gl-spectrum'),
  modules: ['OpenGL']
)

shared_module('gl-spectrum-qt',
  'gl-spectrum.cc',
  dependencies: [audacious_dep, qt_dep, audqt_dep, qt_opengl_dep],
  install: true,
  install_dir: visualization_plugin_dir
)
