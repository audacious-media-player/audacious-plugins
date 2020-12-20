mms_dep = dependency('libmms', version: '>= 0.3', required: false)
have_mms = mms_dep.found()


if have_mms
  shared_module('mms',
    'mms.cc',
    dependencies: [audacious_dep, mms_dep, glib_dep],
    name_prefix: '',
    install: true,
    install_dir: transport_plugin_dir
  )
endif
