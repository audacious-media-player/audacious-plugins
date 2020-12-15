if get_option('ffaudio') == 'ffmpeg'
  conf.set10('HAVE_FFMPEG', true)

  libavcodec_req = '>= 53.40.0'
  libavformat_req = '>= 53.25.0'
  libavutil_req = '>= 51.27.0'
else
  conf.set10('HAVE_LIBAV', true)

  libavcodec_req = '>= 53.25.0'
  libavformat_req = '>= 53.17.0'
  libavutil_req = '>= 51.18.0'
endif


libavcodec_dep = dependency('libavcodec', version: libavcodec_req, required: false)
libavformat_dep = dependency('libavformat', version: libavformat_req, required: false)
libavutil_dep = dependency('libavutil', version: libavutil_req, required: false)


if libavcodec_dep.found() and libavformat_dep.found() and libavutil_dep.found()
  shared_module('ffaudio',
    'ffaudio-core.cc',
    'ffaudio-io.cc',
    dependencies: [audacious_dep, libavcodec_dep, libavformat_dep, libavutil_dep, audtag_dep],
    name_prefix: '',
    install: true,
    install_dir: input_plugin_dir
  )
endif
