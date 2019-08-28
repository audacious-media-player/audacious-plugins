adplug_dep = dependency('adplug', required: false)


if adplug_dep.found()
  shared_module('adplug',
    'adplug-xmms.cc',
    dependencies: [audacious_dep, adplug_dep, audtag_dep],
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
