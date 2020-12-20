modplug_dep = dependency('libmodplug', required: false)
have_modplug = modplug_dep.found()


modplug_archive_sources = [
  'archive/arch_raw.cc',
  'archive/archive.cc',
  'archive/open.cc'
]


modplug_plugin_sources = [
  'modplugbmp.cc',
  'plugin_main.cc'
]


if have_modplug
  shared_module('modplug',
    modplug_archive_sources,
    modplug_plugin_sources,
    dependencies: [audacious_dep, modplug_dep],
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
