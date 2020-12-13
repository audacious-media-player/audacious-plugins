notify_dep = dependency('libnotify', required: false)


if notify_dep.found()
  notify_deps = [audacious_dep, notify_dep]

  if conf.has('USE_QT')
    notify_deps += [qt_dep, audqt_dep]
  endif

  if conf.has('USE_GTK')
    notify_deps += [gtk_dep, audgui_dep]
  endif


  shared_module('notify',
    'event.cc',
    'notify.cc',
    'osd.cc',
    dependencies: notify_deps,
    install: true,
    install_dir: general_plugin_dir
  )
endif
