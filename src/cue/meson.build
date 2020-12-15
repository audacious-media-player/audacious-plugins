cue2_dep = dependency('libcue', version: '>= 2.0', required: false)


if cue2_dep.found()
  cue_dep = cue2_dep
  conf.set10('HAVE_LIBCUE2', true)
else
  cue1_dep = dependency('libcue')

  if cue1_dep.found()
    cue_dep = cue1_dep
  endif
endif


if cue_dep.found()
  shared_module('cue',
    'cue.cc',
    dependencies: [audacious_dep, cue_dep],
    name_prefix: '',
    install: true,
    install_dir: container_plugin_dir
  )
endif
