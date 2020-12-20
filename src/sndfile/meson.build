sndfile_dep = dependency('sndfile', version: '>= 0.19', required: false)
have_sndfile = sndfile_dep.found()


if have_sndfile
  shared_module('sndfile',
    'plugin.cc',
    dependencies: [audacious_dep, sndfile_dep],
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
