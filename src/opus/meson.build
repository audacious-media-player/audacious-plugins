opus_dep = dependency('opus', version: '>= 1.0.1', required: false)
opusfile_dep = dependency('opusfile', version: '>= 0.4', required: false)

have_opus = opus_dep.found() and opusfile_dep.found()


if have_opus
  shared_module('opus',
    'opus.cc',
    dependencies: [audacious_dep, opus_dep, opusfile_dep],
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
