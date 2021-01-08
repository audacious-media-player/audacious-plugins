faad_dep = cxx.find_library('faad', required: false)
have_aac = faad_dep.found() and cxx.has_header('neaacdec.h')


if have_aac
  shared_module('aac-raw',
    'aac.cc',
    dependencies: [audacious_dep, faad_dep, audtag_dep],
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
