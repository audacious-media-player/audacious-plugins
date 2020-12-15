qtnetwork_dep = dependency('qt5', version: qt_req, required: false, modules: ['Network'])


if qtnetwork_dep.found()
  shared_module('streamtuner',
    'streamtuner.cc',
    'shoutcast-model.cc',
    'shoutcast-widget.cc',
    'icecast-widget.cc',
    'icecast-model.cc',
    'ihr-widget.cc',
    'ihr-model.cc',
    dependencies: [qt_dep, qtnetwork_dep, audacious_dep, audqt_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
