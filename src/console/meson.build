gme_dep = dependency('libgme', version: '>= 0.6.0', required: get_option('console'))
have_console = gme_dep.found()


if have_console
  shared_module('console',
    'Audacious_Driver.cc',
    'configure.cc',
    'plugin.cc',
    dependencies: [audacious_dep, gme_dep, zlib_dep],
    name_prefix: '',
    install: true,
    install_dir: input_plugin_dir
  )
endif
