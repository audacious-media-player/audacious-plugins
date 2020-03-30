openmpt_dep = dependency('libopenmpt', version: '>= 0.2', required: false)


if openmpt_dep.found()
  shared_module('openmpt',
    'mpt.cc',
    'mptwrap.cc',
    dependencies: [audacious_dep, openmpt_dep],
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
