gio_dep = dependency('gio-2.0', version: glib_req, required: true)


shared_module('gio',
  'gio.cc',
  dependencies: [audacious_dep, gio_dep],
  install: true,
  install_dir: transport_plugin_dir
)

