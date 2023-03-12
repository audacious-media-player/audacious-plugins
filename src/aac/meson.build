faad_dep = dependency('faad2', required: false)

if not faad_dep.found()
  faad_dep = cxx.find_library('faad', has_headers: 'neaacdec.h', required: false)
endif

have_aac = faad_dep.found()

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
