adplug_dep = dependency('adplug', required: false)
have_adplug = adplug_dep.found()


if have_adplug
  adplug_inc = '-I' + adplug_dep.get_variable(pkgconfig: 'includedir')
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
    name_prefix: '',
    include_directories: [src_inc],
    install: true,
    install_dir: input_plugin_dir,
  )
endif
