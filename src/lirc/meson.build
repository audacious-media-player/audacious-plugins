lirc_dep = dependency('lirc', required: false)

if not lirc_dep.found()
  lirc_dep = cxx.find_library('lirc_client', has_headers: 'lirc/lirc_client.h',
                              required: false)
endif
have_lirc = lirc_dep.found()

if have_lirc
  shared_module('lirc',
    'lirc.cc',
    dependencies: [audacious_dep, glib_dep, lirc_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
