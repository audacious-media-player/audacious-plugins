lirc_dep = cxx.find_library('lirc', required: false)
have_lirc = lirc_dep.found() and cxx.has_header('lirc/lirc_client.h')


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
