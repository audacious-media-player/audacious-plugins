libcdio_dep = dependency('libcdio', version: '>= 0.70', required: false)
libcdio_cdda_dep = dependency('libcdio_cdda', version: '>= 0.70', required: false)
libcddb_dep = dependency('libcddb', version: '>= 1.2.1', required: false)


have_cdaudio = libcdio_dep.found() and \
               libcdio_cdda_dep.found() and \
               libcddb_dep.found()


if have_cdaudio
  shared_module('cdaudio-ng',
    'cdaudio-ng.cc',
    dependencies: [audacious_dep, libcdio_dep, libcdio_cdda_dep, libcddb_dep],
    name_prefix: '',
    install: true,
    install_dir: input_plugin_dir
  )
endif
