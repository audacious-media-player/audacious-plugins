notify_dep = dependency('libnotify', required: false)


if notify_dep.found()
  shared_module('notify',
    'event.cc',
    'notify.cc',
    'osd.cc',
    dependencies: [audacious_dep, notify_dep, qt_dep, audqt_dep],
    install: true,
    install_dir: general_plugin_dir
  )
endif
