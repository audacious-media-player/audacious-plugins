if get_option('gtk3')
  gdk_x11_dep = dependency('gdk-x11-3.0', version: gtk_req, required: false)
else
  gdk_x11_dep = dependency('gdk-x11-2.0', version: gtk_req, required: false)
endif
have_hotkey = gdk_x11_dep.found()


hotkey_sources = [
  'grab.cc',
  'gui.cc',
  'plugin.cc'
]


if have_hotkey
  shared_module('hotkey',
    hotkey_sources,
    dependencies: [audacious_dep, audgui_dep, gtk_dep, glib_dep, gdk_x11_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
