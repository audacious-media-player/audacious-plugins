ogg_dep = dependency('ogg', version: '>= 1.0', required: get_option('vorbis'))
vorbis_dep = dependency('vorbis', version: '>= 1.0', required: get_option('vorbis'))
vorbisenc_dep = dependency('vorbisenc', version: '>= 1.0', required: get_option('vorbis'))
vorbisfile_dep = dependency('vorbisfile', version: '>= 1.0', required: get_option('vorbis'))

have_vorbis = ogg_dep.found() and \
              vorbis_dep.found() and \
              vorbisenc_dep.found() and \
              vorbisfile_dep.found()


if have_vorbis
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
