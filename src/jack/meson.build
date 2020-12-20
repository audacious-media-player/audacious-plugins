jack_dep = dependency('jack', version: '>= 1.9.7', required: false)

if not jack_dep.found()
  jack_dep = dependency('jack', version: ['>= 0.120.1', '< 1.0'], required: false)
endif

have_jack = jack_dep.found()


if have_jack
  shared_module('jack-ng',
    'jack-ng.cc',
    dependencies: [audacious_dep, jack_dep],
    name_prefix: '',
    install: true,
    install_dir: output_plugin_dir
  )
endif
