fluidsynth_dep = dependency('fluidsynth', version: '>= 1.0.6', required: false)
have_amidiplug = fluidsynth_dep.found()


amidiplug_sources = [
  'amidi-plug.cc',
  'backend-fluidsynth/b-fluidsynth.cc',
  'i_midi.cc',
  'i_configure.cc',
  'i_configure-fluidsynth.cc',
  'i_fileinfo.cc'
]


if have_amidiplug
  amidiplug_deps = [audacious_dep, fluidsynth_dep, glib_dep]

  if conf.has('USE_QT')
    amidiplug_deps += [qt_dep, audqt_dep]
  endif

  if conf.has('USE_GTK')
    amidiplug_deps += [gtk_dep, audgui_dep]
  endif


  shared_module('amidi-plug',
    amidiplug_sources,
    dependencies: amidiplug_deps,
    name_prefix: '',
    install: true,
    install_dir: input_plugin_dir
  )
endif
