qt_opengl_dep = dependency('qt5', version: qt_req, modules: ['OpenGL'])


if qt_opengl_dep.found()
  shared_module('gl-spectrum-qt',
    'gl-spectrum.cc',
    dependencies: [audacious_dep, qt_dep, qt_opengl_dep, audqt_dep],
    install: true,
    install_dir: visualization_plugin_dir
  )
endif
