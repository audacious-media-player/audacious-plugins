qt_multimedia_dep = dependency('qt5', version: qt_req, modules: ['Multimedia'])


if qt_multimedia_dep.found()
  shared_module('qtaudio',
    'qtaudio.cc',
    dependencies: [audacious_dep, qt_multimedia_dep],
    install: true,
    install_dir: output_plugin_dir
  )
endif
