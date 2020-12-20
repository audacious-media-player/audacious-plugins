lirc_dep = cxx.find_library('lirc', has_headers: ['lirc/lirc_client.h'], required: false)
have_lirc = lirc_dep.found()


if have_lirc
  shared_module('lirc',
    'lirc.cc',
    dependencies: [audacious_dep, glib_dep, lirc_dep],
    name_prefix: '',
    link_args: ['-llirc_client'],
    install: true,
    install_dir: general_plugin_dir
  )
endif
