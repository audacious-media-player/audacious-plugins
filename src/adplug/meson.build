adplug_dep = dependency('adplug', required: false)


if adplug_dep.found()
  adplug_inc = '-I' + adplug_dep.get_pkgconfig_variable('includedir')
  if cxx.has_header('adplug/nemuopl.h', args: adplug_inc)
    conf.set10('HAVE_ADPLUG_NEMUOPL_H', true)
  endif
  if cxx.has_header('adplug/wemuopl.h', args: adplug_inc)
    conf.set10('HAVE_ADPLUG_WEMUOPL_H', true)
  endif
  if cxx.has_header('adplug/kemuopl.h', args: adplug_inc)
    conf.set10('HAVE_ADPLUG_KEMUOPL_H', true)
  endif


  shared_module('adplug',
    'adplug-xmms.cc',
    dependencies: [audacious_dep, adplug_dep, audtag_dep],
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
