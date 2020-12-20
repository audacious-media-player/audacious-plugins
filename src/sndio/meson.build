sndio_dep = cxx.find_library('sndio', required: false)
have_sndio = sndio_dep.found()


if have_sndio
  shared_module('sndio-ng',
    'sndio.cc',
    dependencies: [audacious_dep, sndio_dep],
    name_prefix: '',
    link_args: ['-lsndio'],
    install: true,
    install_dir: output_plugin_dir
  )
endif
