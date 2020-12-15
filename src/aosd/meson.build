x11_dep = dependency('x11', modules: ['xrender', 'xcomposite'], required: false)


aosd_sources = [
  'aosd.cc',
  'aosd_cfg.cc',
  'aosd_osd.cc',
  'aosd_style.cc',
  'aosd_trigger.cc',
  'aosd_ui.cc',
  'ghosd.c',
  'ghosd-main.c'
]


if x11_dep.found()
  shared_module('aosd',
    aosd_sources,
    dependencies: [audacious_dep, math_dep, gtk_dep, glib_dep, x11_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
