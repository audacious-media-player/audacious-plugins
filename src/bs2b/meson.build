bs2b_dep = dependency('libbs2b', version: '>= 3.0.0', required: false)


if bs2b_dep.found()
  shared_module('bs2b',
    'plugin.cc',
    dependencies: [audacious_dep, bs2b_dep],
    name_prefix: '',
    install: true,
    install_dir: effect_plugin_dir
  )
endif
