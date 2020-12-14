lirc_dep = cxx.find_library('lirc', has_headers: ['lirc/lirc_client.h'], required: false)


if lirc_dep.found()
  shared_module('lirc',
    'lirc.cc',
    dependencies: [audacious_dep, glib_dep, lirc_dep],
    link_args: ['-llirc_client'],
    install: true,
    install_dir: general_plugin_dir
  )
endif
