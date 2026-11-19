avformat_dep   = dependency('libavformat')
avcodec_dep    = dependency('libavcodec')
avutil_dep     = dependency('libavutil')
swresample_dep = dependency('libswresample')
threads_dep    = dependency('threads')
shared_module('waveform-qt',
  'track_peaks.cc',
  'preload.cc',
  'track_state.cc',
  'waveform_widget.cc',
  'settings.cc',
  'waveform_plugin.cc',
  dependencies: [audacious_dep, qt_dep, avformat_dep, avcodec_dep, avutil_dep, swresample_dep, threads_dep],
  name_prefix: '',
  install: true,
  install_dir: visualization_plugin_dir
)
