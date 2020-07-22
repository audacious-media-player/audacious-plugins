fluidsynth_dep = dependency('fluidsynth', version: '>= 1.0.6', required: false)


amidiplug_sources = [
  'amidi-plug.cc',
  'backend-fluidsynth/b-fluidsynth.cc',
  'i_midi.cc',
  'i_configure.cc',
  'i_configure-fluidsynth.cc',
  'i_fileinfo.cc'
]


if fluidsynth_dep.found()
  shared_module('amidi-plug',
    amidiplug_sources,
    dependencies: [audacious_dep, fluidsynth_dep, glib_dep, qt_dep, audqt_dep],
    install: true,
    install_dir: input_plugin_dir
  )
endif
