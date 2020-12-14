jack_dep = dependency('jack', version: '>= 1.9.7', required: false)

if not jack_dep.found()
  jack_dep = dependency('jack', version: ['>= 0.120.1', '< 1.0'], required: false)
endif


if jack_dep.found()
  shared_module('jack-ng',
    'jack-ng.cc',
    dependencies: [audacious_dep, jack_dep],
    install: true,
    install_dir: output_plugin_dir
  )
endif
