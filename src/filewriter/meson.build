filewriter_deps = [audacious_dep, glib_dep]
filewriter_srcs = [
  'convert.cc',
  'filewriter.cc',
  'wav.cc'
]

if get_option('filewriter-flac') and flac_dep.found()
  filewriter_deps += [flac_dep]
  filewriter_srcs += ['flac.cc']

  conf.set10('FILEWRITER_FLAC', true)
endif

if get_option('filewriter-ogg') and ogg_dep.found()
  filewriter_deps += [ogg_dep, vorbis_dep, vorbisenc_dep, vorbisfile_dep]
  filewriter_srcs += ['vorbis.cc']

  conf.set10('FILEWRITER_VORBIS', true)
endif

if get_option('filewriter-mp3')
  lame_dep = cxx.find_library('mp3lame', has_headers: ['lame/lame.h'])
  filewriter_deps += [lame_dep]
  filewriter_srcs += ['mp3.cc']

  conf.set10('FILEWRITER_MP3', true)
endif

shared_module('filewriter',
  filewriter_srcs,
  dependencies: filewriter_deps,
  install: true,
  install_dir: output_plugin_dir
)
