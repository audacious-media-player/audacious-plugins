x11_dep = dependency('x11', required: false, modules: ['xrender', 'xcomposite'])
have_aosd = x11_dep.found()


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


if have_aosd
  shared_module('aosd',
    aosd_sources,
    dependencies: [audacious_dep, math_dep, gtk_dep, glib_dep, x11_dep],
    name_prefix: '',
    install: true,
    install_dir: general_plugin_dir
  )
endif
