qtnetwork_dep = dependency('qt5', version: qt_req, required: true, modules: ['Network'])


if qtnetwork_dep.found()
  shared_module('streamtuner',
    'streamtuner.cc',
    'shoutcast-model.cc',
    'shoutcast-widget.cc',
    'icecast-widget.cc',
    'icecast-model.cc',
    dependencies: [qt_dep, qtnetwork_dep, audacious_dep, audqt_dep],
    install: true,
    install_dir: general_plugin_dir
  )
endif
