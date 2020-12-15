alsa_dep = dependency('alsa', version: '>= 1.0.16')


if alsa_dep.found()
  shared_module('alsa',
    'alsa.cc',
    'config.cc',
    dependencies: [audacious_dep, alsa_dep, glib_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
