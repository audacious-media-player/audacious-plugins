sndio_dep = dependency('sndio', required: false)
have_sndio = sndio_dep.found()


if sndio_dep.version().version_compare('>= 1.9.0')
  conf.set10('HAVE_SNDIO_1_9', true)
endif


if have_sndio
  shared_module('sndio-ng',
    'sndio.cc',
    dependencies: [audacious_dep, sndio_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
