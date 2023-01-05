if get_option('qt6')
  qt_multimedia_dep = dependency('qt6', version: qt_req, required: false, modules: ['Multimedia'])

  if qt_multimedia_dep.found()
    warning('The Qt Multimedia output plugin is not yet compatible with Qt 6.')
    qt_multimedia_dep = dependency('', required: false)
  endif
else
  qt_multimedia_dep = dependency('qt5', version: qt_req, required: false, modules: ['Multimedia'])
endif

have_qtaudio = qt_multimedia_dep.found()


if have_qtaudio
  shared_module('qtaudio',
    'qtaudio.cc',
    dependencies: [audacious_dep, qt_multimedia_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
