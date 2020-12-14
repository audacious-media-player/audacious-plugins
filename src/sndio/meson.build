sndio_dep = cxx.find_library('sndio', required: false)


if sndio_dep.found()
  shared_module('sndio-ng',
    'sndio.cc',
    dependencies: [audacious_dep, sndio_dep],
    link_args: ['-lsndio'],
    install: true,
    install_dir: output_plugin_dir
  )
endif
