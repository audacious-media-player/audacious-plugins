flac_dep = dependency('flac', version: '>= 1.2.1')


if flac_dep.found()
  shared_module('flacng',
    'plugin.cc',
    'tools.cc',
    'seekable_stream_callbacks.cc',
    'metadata.cc',
    dependencies: [audacious_dep, flac_dep],
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif

