pipewire_dep = dependency('libpipewire-0.3', version: '>= 0.3.26', required: false)
spa_dep = dependency('libspa-0.2', version: '>= 0.2', required: false)
have_pipewire = pipewire_dep.found() and spa_dep.found()


if have_pipewire
  shared_module('pipewire',
    'pipewire.cc',
    dependencies: [audacious_dep, pipewire_dep, spa_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
