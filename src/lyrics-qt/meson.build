lyrics_deps = [audacious_dep, audqt_dep, glib_dep, qt_dep]
lyrics_src = [
  '../lyrics-common/file_provider.cc',
  '../lyrics-common/lrclib_provider.cc',
  '../lyrics-common/lyrics_ovh_provider.cc',
  '../lyrics-common/utils.cc',
  'lyrics-qt.cc'
]

shared_module('lyrics-qt',
  lyrics_src,
  dependencies: lyrics_deps,
  name_prefix: '',
  install: true,
  install_dir: general_plugin_dir
)
