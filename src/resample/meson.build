if samplerate_dep.found()
  shared_module('resample',
    'resample.cc',
    include_directories: [src_inc],
    dependencies: [audacious_dep, samplerate_dep],
    name_prefix: '',
    install: true,
    install_dir: effect_plugin_dir
  )
endif
