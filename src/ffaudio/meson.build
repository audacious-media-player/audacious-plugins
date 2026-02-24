libavcodec_dep = dependency('libavcodec', version: '>= 58.54.100', required: false)
libavformat_dep = dependency('libavformat', version: '>= 58.29.100', required: false)
libavutil_dep = dependency('libavutil', version: '>= 56.31.100', required: false)


have_ffaudio = libavcodec_dep.found() and \
               libavformat_dep.found() and \
               libavutil_dep.found()


if have_ffaudio
  shared_module('ffaudio',
    'ffaudio-core.cc',
    'ffaudio-io.cc',
    dependencies: [audacious_dep, libavcodec_dep, libavformat_dep, libavutil_dep, audtag_dep],
    name_prefix: '',
    install: true,
    install_dir: input_plugin_dir
  )
endif
