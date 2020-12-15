filewriter_deps = [audacious_dep, glib_dep]
filewriter_srcs = [
  'convert.cc',
  'filewriter.cc',
  'wav.cc'
]


if get_option('filewriter-flac')
  flac_dep = dependency('flac', version: '>= 1.2.1', required: false)

  if flac_dep.found()
    filewriter_deps += [flac_dep]
    filewriter_srcs += ['flac.cc']

    conf.set10('FILEWRITER_FLAC', true)
  endif
endif


if get_option('filewriter-ogg')
  ogg_dep = dependency('ogg', version: '>= 1.0', required: false)
  vorbis_dep = dependency('vorbis', version: '>= 1.0', required: false)
  vorbisenc_dep = dependency('vorbisenc', version: '>= 1.0', required: false)
  vorbisfile_dep = dependency('vorbisfile', version: '>= 1.0', required: false)

  if ogg_dep.found() and vorbis_dep.found()
    filewriter_deps += [ogg_dep, vorbis_dep, vorbisenc_dep, vorbisfile_dep]
    filewriter_srcs += ['vorbis.cc']

    conf.set10('FILEWRITER_VORBIS', true)
  endif
endif


if get_option('filewriter-mp3')
  lame_dep = cxx.find_library('mp3lame', has_headers: ['lame/lame.h'], required: false)

  if lame_dep.found()
    filewriter_deps += [lame_dep]
    filewriter_srcs += ['mp3.cc']

    conf.set10('FILEWRITER_MP3', true)
  endif
endif


shared_module('filewriter',
  filewriter_srcs,
  dependencies: filewriter_deps,
  name_prefix: '',
  install: true,
  install_dir: output_plugin_dir
)
