mms_dep = dependency('libmms', version: '>= 0.3', required: false)


if mms_dep.found()
  shared_module('mms',
    'mms.cc',
    dependencies: [audacious_dep, mms_dep, glib_dep],
    install: true,
    install_dir: transport_plugin_dir
  )
endif
