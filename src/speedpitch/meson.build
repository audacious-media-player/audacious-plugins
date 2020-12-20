have_speedpitch = samplerate_dep.found()


if have_speedpitch
  shared_module('speed-pitch',
    'speed-pitch.cc',
    include_directories: [src_inc],
    dependencies: [audacious_dep, samplerate_dep],
    name_prefix: '',
    install: true,
    install_dir: effect_plugin_dir
  )
endif
