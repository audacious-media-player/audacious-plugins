sndio_dep = dependency('sndio', version: '>= 1.9.0', required: false)
have_sndio = sndio_dep.found()


if have_sndio
  shared_module('sndio-ng',
    'sndio.cc',
    dependencies: [audacious_dep, sndio_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
