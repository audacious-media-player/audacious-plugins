ogg_dep = dependency('ogg', version: '>= 1.0')
vorbis_dep = dependency('vorbis', version: '>= 1.0')
vorbisenc_dep = dependency('vorbisenc', version: '>= 1.0')
vorbisfile_dep = dependency('vorbisfile', version: '>= 1.0')


if ogg_dep.found()
  shared_module('vorbis',
    'vcupdate.cc',
    'vcedit.cc',
    'vorbis.cc',
    dependencies: [audacious_dep, ogg_dep, vorbis_dep, vorbisenc_dep, vorbisfile_dep, glib_dep],
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
