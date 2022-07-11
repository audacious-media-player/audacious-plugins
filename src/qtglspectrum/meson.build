if get_option('qt6')
  qt_opengl_dep = dependency('qt6', version: qt_req, required: false, modules: ['OpenGL', 'OpenGLWidgets'])
else
  qt_opengl_dep = dependency('qt5', version: '>= 5.4', required: false, modules: ['OpenGL'])
endif

have_qtglspectrum = qt_opengl_dep.found()


if have_qtglspectrum
  shared_module('gl-spectrum-qt',
    'gl-spectrum.cc',
    dependencies: [audacious_dep, qt_dep, audqt_dep, qt_opengl_dep],
    name_prefix: '',
    install: true,
    install_dir: visualization_plugin_dir
  )
endif
