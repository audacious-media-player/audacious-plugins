notify_dep = dependency('libnotify', version: '>= 0.7', required: false)
pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.26', required: false)

if notify_dep.found() and pixbuf_dep.found()
  notify_deps = [audacious_dep, notify_dep, pixbuf_dep]

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
