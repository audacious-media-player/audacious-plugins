faad_dep = cxx.find_library('faad', has_headers: ['neaacdec.h'])


if faad_dep.found()
  shared_module('aac-raw',
    'aac.cc',
    dependencies: [audacious_dep, faad_dep, audtag_dep],
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif

