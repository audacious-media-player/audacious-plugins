libcdio_dep = dependency('libcdio', version: '>= 0.70', required: false)
libcdio_cdda_dep = dependency('libcdio_cdda', version: '>= 0.70', required: false)

have_cdaudio = libcdio_dep.found() and libcdio_cdda_dep.found()
cdaudio_deps = [audacious_dep, libcdio_dep, libcdio_cdda_dep]

if get_option('cdaudio-cddb') and have_cdaudio
  libcddb_dep = dependency('libcddb', version: '>= 1.2.1', required: false)

  if libcddb_dep.found()
    cdaudio_deps += libcddb_dep
    conf.set10('HAVE_LIBCDDB', true)
  else
    warning('CDDB support for the Audio CD plugin was disabled due to missing dependency.')
  endif
endif

if have_cdaudio
  shared_module('cdaudio-ng',
    'cdaudio-ng.cc',
    dependencies: cdaudio_deps,
    name_prefix: '',
    install: true,
    install_dir: input_plugin_dir
  )
endif
