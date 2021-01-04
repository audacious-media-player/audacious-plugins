qt_opengl_dep = dependency('qt5', version: '>= 5.4', required: false, modules: ['OpenGL'])
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
