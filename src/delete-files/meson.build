delete_files_deps = [audacious_dep, gio_dep]

if conf.has('USE_QT')
  delete_files_deps += [qt_dep, audqt_dep]
endif

if conf.has('USE_GTK')
  delete_files_deps += [gtk_dep, audgui_dep]
endif


shared_module('delete-files',
  'delete-files.cc',
  dependencies: delete_files_deps,
  install: true,
  install_dir: general_plugin_dir
)
