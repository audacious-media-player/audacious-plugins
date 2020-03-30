wavpack_dep = dependency('wavpack', version: '>= 4.31', required: false)


if wavpack_dep.found()
  shared_module('wavpack',
    'wavpack.cc',
    dependencies: [audacious_dep, wavpack_dep, audtag_dep],
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
