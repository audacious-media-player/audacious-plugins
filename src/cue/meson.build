cue_dep = dependency('libcue', required: false)
have_cue = cue_dep.found()


if cue_dep.version().version_compare('>= 2.0')
  conf.set10('HAVE_LIBCUE2', true)
endif


if have_cue
  shared_module('cue',
    'cue.cc',
    dependencies: [audacious_dep, cue_dep],
    name_prefix: '',
    install: true,
    install_dir: container_plugin_dir
  )
endif
