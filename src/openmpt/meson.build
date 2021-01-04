openmpt_dep = dependency('libopenmpt', version: '>= 0.2', required: false)
have_openmpt = openmpt_dep.found()


if have_openmpt
  shared_module('openmpt',
    'mpt.cc',
    'mptwrap.cc',
    dependencies: [audacious_dep, openmpt_dep],
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
